; Open-Meteo screen. Loading defines code only; it never fetches or runs.
; Before app-run: (weather-config LATITUDE LONGITUDE 'celsius) or 'fahrenheit.
; No default location. Only explicit app-run starts the first HTTPS GET.
; Data is RAM-only. r refreshes (60s minimum); q exits and cancels own IO.

(define weather-settings nil)
(define weather-data nil)
(define weather-status 'unconfigured)
(define weather-error nil)
(define weather-id nil)
(define weather-next-ms 0)
(define weather-manual-ms 0)
(define weather-start-ms 0)
(define weather-fetch-ms 0)
(define weather-dirty t)
(define weather-last-minute -1)
(define weather-last-stale nil)
; Decode the unit once without putting non-ASCII bytes in the source file.
(define weather-c-unit (ix (json-parse "\"\\u00b0C\"") 1))

(define weather-range
  (lambda (v low high) (and (number? v) (>= v low) (<= v high))))
(define weather-integer
  (lambda (v low high)
    (and (weather-range v low high) (= v (to-i64 v)))))
(define weather-config
  (lambda (latitude longitude unit)
    (if (and (weather-range latitude -90 90)
             (weather-range longitude -180 180)
             (or (eq unit 'celsius) (eq unit 'fahrenheit)))
      (progn (setq weather-settings (list latitude longitude unit)) t)
      nil)))
(define weather-get
  (lambda (o key)
    (if (and (list? o) (eq (car o) 'json-object))
      (json-get o key) 'json-missing)))
(define weather-array
  (lambda (a count)
    (and (list? a) (eq (car a) 'json-array) (= (length (cdr a)) count))))
(define weather-first
  (lambda (a) (if (weather-array a 1) (ix (cdr a) 0) 'json-missing)))
(define weather-nullable
  (lambda (v low high) (or (eq v 'json-null) (weather-range v low high))))
(define weather-option
  (lambda (v) (if (number? v) v nil)))
(define weather-pad
  (lambda (n) (str-join (list (if (< n 10) "0" "") (str-from-n n)))))
(define weather-date
  (lambda (day)
    (let ((z (+ day 719468)))
      (let ((era (// z 146097)))
        (let ((doe (- z (* era 146097))))
          (let ((yoe (// (- (+ (- doe (// doe 1460)) (// doe 36524))
                           (// doe 146096)) 365)))
            (let ((doy (- doe (+ (* 365 yoe) (// yoe 4) (- (// yoe 100)))))
                  (year (+ yoe (* era 400))))
              (let ((mp (// (+ (* 5 doy) 2) 153)))
                (let ((d (+ (- doy (// (+ (* 153 mp) 2) 5)) 1))
                      (m (+ mp (if (< mp 10) 3 -9))))
                  (str-join (list (str-from-n (+ year (if (<= m 2) 1 0)))
                                  "-" (weather-pad m) "-" (weather-pad d))))))))))))
(define weather-hm
  (lambda (stamp offset)
    (let ((seconds (mod (+ stamp offset) 86400)))
      (str-join (list (weather-pad (// seconds 3600)) ":"
                     (weather-pad (// (mod seconds 3600) 60)))))))

(define weather-series
  (lambda (times temps rain start i)
    (if (= i 24) t
      (and (weather-integer (car times) 1735689600i64 4102444800i64)
           (= (car times) (+ start (* i 3600)))
           (weather-nullable (car temps) -100 70)
           (weather-nullable (car rain) 0 100)
           (weather-series (cdr times) (cdr temps) (cdr rain) start (+ i 1))))))
(define weather-rain
  (lambda (times rain current best)
    (if (eq times nil) best
      (if (>= (car times) current)
        (if (eq (car rain) 'json-null) nil
          (weather-rain (cdr times) (cdr rain) current
            (if (or (eq best nil) (> (car rain) best)) (car rain) best)))
        (weather-rain (cdr times) (cdr rain) current best)))))
(define weather-sample
  (lambda (temps rain hour)
    (list (weather-option (ix temps hour)) (weather-option (ix rain hour)))))
; Snapshot: timestamp, offset, local day, temp, feels, WMO, low, high,
; three samples, remaining rain max, date, model HH:MM, initial age ms.
(define weather-snapshot
  (lambda (doc wall)
    (let ((current (weather-get doc "current"))
          (daily (weather-get doc "daily"))
          (hourly (weather-get doc "hourly"))
          (cu (weather-get doc "current_units"))
          (du (weather-get doc "daily_units"))
          (hu (weather-get doc "hourly_units")))
      (let ((stamp (weather-get current "time"))
            (temp (weather-get current "temperature_2m"))
            (feels (weather-get current "apparent_temperature"))
            (code (weather-get current "weather_code"))
            (offset (weather-get doc "utc_offset_seconds"))
            (start (weather-first (weather-get daily "time")))
            (low (weather-first (weather-get daily "temperature_2m_min")))
            (high (weather-first (weather-get daily "temperature_2m_max")))
            (times (weather-get hourly "time"))
            (temps (weather-get hourly "temperature_2m"))
            (rain (weather-get hourly "precipitation_probability")))
        (if (and wall
                 (weather-integer stamp 1735689600i64 4102444800i64)
                 (weather-integer start 1735689600i64 4102444800i64)
                 (weather-integer offset -50400 50400)
                 (weather-range temp -100 70)
                 (or (eq feels 'json-missing) (weather-nullable feels -100 70))
                 (or (eq code 'json-null) (eq code 'json-missing)
                     (weather-integer code 0 999))
                 (weather-range low -100 70) (weather-range high low 70)
                 (<= (- wall stamp) 7200) (<= (- stamp wall) 900)
                 (= (mod (+ start offset) 86400) 0)
                 (= (// (+ start offset) 86400) (// (+ stamp offset) 86400))
                 (= (// (+ start offset) 86400) (// (+ wall offset) 86400))
                 (eq (weather-get cu "time") "unixtime")
                 (eq (weather-get du "time") "unixtime")
                 (eq (weather-get hu "time") "unixtime")
                 (eq (weather-get cu "temperature_2m") weather-c-unit)
                 (or (not (number? feels))
                     (eq (weather-get cu "apparent_temperature") weather-c-unit))
                 (eq (weather-get du "temperature_2m_min") weather-c-unit)
                 (eq (weather-get du "temperature_2m_max") weather-c-unit)
                 (eq (weather-get hu "temperature_2m") weather-c-unit)
                 (eq (weather-get hu "precipitation_probability") "%")
                 (weather-array times 24) (weather-array temps 24) (weather-array rain 24)
                 (weather-series (cdr times) (cdr temps) (cdr rain) start 0))
          (list stamp offset (// (+ start offset) 86400) temp
                (weather-option feels) (weather-option code) low high
                (map (lambda (h) (weather-sample (cdr temps) (cdr rain) h)) '(7 12 18))
                (weather-rain (cdr times) (cdr rain) stamp nil)
                (weather-date (// (+ start offset) 86400)) (weather-hm stamp offset)
                (* 1000 (if (> wall stamp) (- wall stamp) 0)))
          nil)))))

(define weather-unit
  (lambda () (if (eq (ix weather-settings 2) 'fahrenheit) "F" "C")))
(define weather-temperature
  (lambda (v)
    (if (eq v nil) nil
      (if (eq (ix weather-settings 2) 'fahrenheit) (+ (/ (* v 9.0f64) 5) 32) v))))
(define weather-round
  (lambda (v) (to-i (if (< v 0) (- v 0.5) (+ v 0.5)))))
(define weather-number-text
  (lambda (v) (if (eq v nil) "--" (str-from-n (weather-round v)))))
(define weather-temp-text
  (lambda (v) (weather-number-text (weather-temperature v))))
(define weather-condition
  (lambda (value)
    (let ((code (if (number? value) (to-i value) -1)))
      (if (eq code 0) "CLEAR"
        (if (eq code 1) "MAINLY CLEAR"
          (if (eq code 2) "PARTLY CLOUDY"
            (if (eq code 3) "OVERCAST"
              (if (member code '(45 48)) "FOG"
                (if (member code '(51 53 55)) "DRIZZLE"
                  (if (member code '(56 57)) "FREEZING DRIZZLE"
                    (if (member code '(61 63 65)) "RAIN"
                      (if (member code '(66 67)) "FREEZING RAIN"
                        (if (member code '(71 73 75 77 85 86)) "SNOW"
                          (if (member code '(80 81 82)) "SHOWERS"
                            (if (member code '(95 96 99)) "THUNDERSTORM" "UNKNOWN")))))))))))))))
(define weather-age
  (lambda ()
    (if weather-data (+ (- (monotonic-ms) weather-fetch-ms) (ix weather-data 12)) 0)))
(define weather-time-stale
  (lambda ()
    (let ((wall (time-unix)))
      (and weather-data
           (or (eq wall nil)
               (not (= (// (+ wall (ix weather-data 1)) 86400) (ix weather-data 2)))
               (> (- (ix weather-data 0) wall) 900))))))
(define weather-stale
  (lambda ()
    (if (and weather-data
             (or weather-error (>= (weather-age) 3600000) (weather-time-stale))) t nil)))
(define weather-fail
  (lambda (message)
    (progn (setq weather-error message) (setq weather-status 'error) (setq weather-dirty t))))
(define weather-fetch
  (lambda (now)
    (progn
      (setq weather-next-ms (+ now 1800000))
      (setq weather-manual-ms (+ now 60000))
      (let ((reply (http-get (str-join
              (list "https://api.open-meteo.com/v1/forecast?latitude="
                    (str-from-n (ix weather-settings 0) 4) "&longitude="
                    (str-from-n (ix weather-settings 1) 4)
                    "&current=temperature_2m,apparent_temperature,weather_code"
                    "&hourly=temperature_2m,precipitation_probability"
                    "&daily=temperature_2m_min,temperature_2m_max"
                    "&temperature_unit=celsius&timezone=auto&timeformat=unixtime&forecast_days=1")))))
        (if (eq (ix reply 0) 'ok)
          (progn (setq weather-id (ix reply 1)) (setq weather-start-ms now)
                 (setq weather-status 'loading) (setq weather-dirty t))
          (weather-fail (sym2str (ix reply 1))))))))
(define weather-poll
  (lambda (now)
    (if (>= (- now weather-start-ms) 35000)
      (progn (http-release weather-id) (setq weather-id nil) (weather-fail "timeout"))
      (let ((reply (http-poll weather-id)))
        (if (eq (ix reply 0) 'pending) t
          (progn
            (http-release weather-id) (setq weather-id nil)
            (if (eq (ix reply 0) 'ok)
              (let ((parsed (json-parse (ix reply 2))) (wall (time-unix)))
                (let ((data (if (eq (ix parsed 0) 'ok)
                              (weather-snapshot (ix parsed 1) wall) nil)))
                  (if data
                    (progn (setq weather-data data) (setq weather-fetch-ms now)
                           (setq weather-status 'ready) (setq weather-error nil)
                           (setq weather-dirty t))
                    (weather-fail (if (eq wall nil) "clock" "bad-data")))))
              (weather-fail
                (if (eq (ix reply 1) 'http-status)
                  (str-join (list "HTTP " (str-from-n (ix reply 2))))
                  (sym2str (ix reply 1)))))))))))

(define weather-segments
  '((1 4 0 16 4) (2 20 4 4 16) (4 20 24 4 16) (8 4 40 16 4)
    (16 0 24 4 16) (32 0 4 4 16) (64 4 20 16 4)))
(define weather-digit
  (lambda (x y n)
    (let ((mask (ix '(63 6 91 79 102 109 125 7 127 111) n)))
      (map (lambda (s)
             (if (not (= (bitwise-and mask (ix s 0)) 0))
               (gfx-rect (+ x (ix s 1)) (+ y (ix s 2)) (ix s 3) (ix s 4) 65535) t))
           weather-segments))))
(define weather-big-positive
  (lambda (x y n)
    (if (< n 10) (progn (weather-digit x y n) (+ x 28))
      (let ((right (weather-big-positive x y (// n 10))))
        (progn (weather-digit right y (mod n 10)) (+ right 28))))))
(define weather-big
  (lambda (v)
    (if (eq v nil)
      (progn (gfx-rect 16 72 16 4 33808) (gfx-rect 44 72 16 4 33808))
      (let ((n (weather-round (weather-temperature v))))
        (if (< n 0)
          (progn (gfx-rect 12 72 16 4 65535) (weather-big-positive 36 52 (- n)))
          (weather-big-positive 12 52 n))))))
(define weather-draw
  (lambda ()
    (let ((data weather-data) (unit (weather-unit)) (stale (weather-stale))
          (age (// (weather-age) 60000)))
      (progn
        (gfx-clear 2)
        (gfx-text 12 8 "WEATHER" 65535)
        (gfx-text 228 8 (if data (ix data 10) "----------") 65535)
        (gfx-text 12 28
          (if weather-settings
            (str-join (list "Lat " (str-from-n (ix weather-settings 0) 3)
                           " Lon " (str-from-n (ix weather-settings 1) 3)))
            "Location not configured") 33808)
        (weather-big (if data (ix data 3) nil))
        (gfx-text 142 82 unit 65535)
        (gfx-text 180 54 (if data (weather-condition (ix data 5)) "NO FORECAST") 65535)
        (gfx-text 180 82
          (str-join (list "Feels " (weather-temp-text (if data (ix data 4) nil)) " " unit)) 33808)
        (gfx-text 12 112
          (str-join (list "Low " (weather-temp-text (if data (ix data 6) nil))
                         " / High " (weather-temp-text (if data (ix data 7) nil)) " " unit)) 65535)
        (gfx-line 12 132 308 132 16904)
        (map (lambda (i)
               (let ((x (+ 12 (* i 104))) (sample (if data (ix (ix data 8) i) nil)))
                 (progn
                   (gfx-text x 142 (ix '("07:00" "12:00" "18:00") i) 33808)
                   (gfx-text x 158 (str-join (list (weather-temp-text (ix sample 0)) " " unit)) 65535)
                   (gfx-text x 176 (str-join (list "Rain " (weather-number-text (ix sample 1)) "%"))
                     (if (and (number? (ix sample 1)) (>= (ix sample 1) 50)) 65504 33808)))))
             '(0 1 2))
        (gfx-text 12 196
          (if stale "STALE - CHECK A CURRENT FORECAST"
            (if data (if (eq (ix data 9) nil) "RAIN DATA MISSING"
                       (if (>= (ix data 9) 50) "BRING AN UMBRELLA" "RAIN CHANCE LOW"))
              (if weather-settings "WAITING FOR DATA" "Use weather-config before app-run")))
          (if stale 63488 (if (and data (number? (ix data 9)) (< (ix data 9) 50)) 2016 65504)))
        (gfx-text 12 212
          (if weather-error (str-join (list (if data "STALE: " "Error: ") weather-error))
            (if stale
              (if (weather-time-stale) "STALE: clock/date mismatch"
                (str-join (list "STALE: age " (str-from-n age) "m")))
              (if (eq weather-status 'loading) "Fetching forecast..."
                (if data (str-join (list "As of " (ix data 11) "  +" (str-from-n age) "m"))
                  "No network request until configured"))))
          (if (or stale weather-error) 63488 33808))
        (gfx-text 12 228 "Open-Meteo.com | r refresh | q exit" 33808)))))
(define weather-paint
  (lambda ()
    (progn
      (setq weather-last-minute (// (weather-age) 60000))
      (setq weather-last-stale (weather-stale))
      (weather-draw) (setq weather-dirty nil))))
(define weather
  (list
    (lambda ()
      (progn
        (setq weather-data nil) (setq weather-error nil) (setq weather-id nil)
        (setq weather-fetch-ms (monotonic-ms)) (setq weather-dirty t)
        (setq weather-status 'unconfigured)
        (if weather-settings (weather-fetch (monotonic-ms)) t)
        (weather-paint)))
    (lambda (event)
      (if (= event 113) 'exit
        (let ((now (monotonic-ms)))
          (progn
            (if weather-id (weather-poll now) t)
            (if (and weather-settings (eq weather-id nil)
                     (or (>= now weather-next-ms)
                         (and (= event 114) (>= now weather-manual-ms))))
              (weather-fetch now) t)
            (if (or weather-dirty
                    (not (= (// (weather-age) 60000) weather-last-minute))
                    (not (eq (weather-stale) weather-last-stale)))
              (weather-paint) t)))))
    (lambda ()
      (progn (if weather-id (http-release weather-id) t) (setq weather-id nil) t))))
