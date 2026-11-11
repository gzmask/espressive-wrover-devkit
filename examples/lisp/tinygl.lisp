; TinyGL-style software wireframe demo, not the TinyGL library or OpenGL.
; Load through app-load-url; wait for loaded, then submit (app-run tinygl).
; UART p/SPACE pauses, q exits. TCP :key p / :key q; :stop also exits.

(define tinygl-phase 0)
(define tinygl-paused nil)
(define tinygl-sines
  '(0 200 392 569 724 851 946 1004
    1024 1004 946 851 724 569 392 200
    0 -200 -392 -569 -724 -851 -946 -1004
    -1024 -1004 -946 -851 -724 -569 -392 -200))
(define tinygl-vertices
  '((-40 -40 -40) (40 -40 -40) (40 40 -40) (-40 40 -40)
    (-40 -40 40) (40 -40 40) (40 40 40) (-40 40 40)))
(define tinygl-edges
  '((0 1 2047) (1 2 2047) (2 3 2047) (3 0 2047)
    (4 5 63519) (5 6 63519) (6 7 63519) (7 4 63519)
    (0 4 65535) (1 5 65535) (2 6 65535) (3 7 65535)))

(define tinygl-project
  (lambda (vertex)
    (let ((x (ix vertex 0))
          (y (ix vertex 1))
          (z (ix vertex 2))
          (s (ix tinygl-sines tinygl-phase))
          (c (ix tinygl-sines (mod (+ tinygl-phase 8) 32))))
      ; Y rotation, fixed X tilt (cos=4/5, sin=3/5), then perspective.
      (let ((rx (/ (- (* x c) (* z s)) 1024))
            (rz (/ (+ (* x s) (* z c)) 1024)))
        (let ((ry (/ (- (* 4 y) (* 3 rz)) 5))
              (depth (+ 240 (/ (+ (* 3 y) (* 4 rz)) 5))))
          (list (+ 160 (/ (* 200 rx) depth))
                (+ 124 (/ (* 200 ry) depth))))))))

(define tinygl-draw
  (lambda ()
    (let ((points (map tinygl-project tinygl-vertices)))
      (progn
        (gfx-clear 2)
        (gfx-text 16 12 "Tiny GL / software 3D" 65535)
        (gfx-text 16 216 "p/SPACE pause / q exit" 33808)
        (map (lambda (edge)
               (let ((a (ix points (ix edge 0)))
                     (b (ix points (ix edge 1))))
                 (gfx-line (ix a 0) (ix a 1) (ix b 0) (ix b 1) (ix edge 2))))
             tinygl-edges)
        (if tinygl-paused (gfx-text 240 12 "PAUSED" 65504) t)))))

(define tinygl
  (list
    (lambda ()
      (progn
        (setq tinygl-phase 0)
        (setq tinygl-paused nil)
        (tinygl-draw)))
    (lambda (event)
      (if (= event 113)
        'exit
        (if (or (= event 112) (= event 32))
          (progn
            (setq tinygl-paused (not tinygl-paused))
            (tinygl-draw))
          (if (and (= event -1) (not tinygl-paused))
            (progn
              (setq tinygl-phase (mod (+ tinygl-phase 1) 32))
              (tinygl-draw))
            t))))
    (lambda () t)))
