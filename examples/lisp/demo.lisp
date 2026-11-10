(define demo
  ; Load this definition through the existing REPL, then submit (app-run demo).
  ; No auto-run, filesystem or persistent storage. UART q / TCP :key q exits.
  (list
    (lambda ()
      (progn
        (gfx-clear 0)
        (gfx-text 20 12 "LispBM application" 65535)
        (gfx-rect 20 40 120 60 63488)
        (gfx-line 20 125 300 125 2047)
        (gfx-text 20 150 "Press q to return to terminal" 65535)))
    (lambda (event)
      (if (= event 113)
        'exit
        (if (= event -1)
          t
          (progn
            (gfx-rect 20 185 280 24 2016)
            (gfx-text 24 190 "Key received; q exits" 0)))))
    (lambda () t)))
