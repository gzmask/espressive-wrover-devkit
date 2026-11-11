; Minimal 2D animation baseline for comparing against tinygl.lisp.
; It uses the same app callback, double canvas and full LCD frame path,
; but each frame only clears, draws one rectangle and writes fixed text.

(define tiny2d-phase 0)
(define tiny2d-paused nil)

(define tiny2d-draw
  (lambda ()
    (progn
      (gfx-clear 2)
      (gfx-text 12 12 "2D baseline" 65535)
      (gfx-text 16 216 "p/SPACE pause / q exit" 33808)
      (gfx-rect (+ 24 (* (mod tiny2d-phase 28) 9)) 104 48 32 2016)
      (if tiny2d-paused (gfx-text 240 12 "PAUSED" 65504) t))))

(define tiny2d
  (list
    (lambda ()
      (progn
        (setq tiny2d-phase 0)
        (setq tiny2d-paused nil)
        (tiny2d-draw)))
    (lambda (event)
      (if (= event 113)
        'exit
        (if (or (= event 112) (= event 32))
          (progn
            (setq tiny2d-paused (not tiny2d-paused))
            (tiny2d-draw))
          (if (and (= event -1) (not tiny2d-paused))
            (progn
              (setq tiny2d-phase (mod (+ tiny2d-phase 1) 32))
              (tiny2d-draw))
            t))))
    (lambda () t)))
