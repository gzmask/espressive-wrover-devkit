#!/usr/bin/env bb
(ns ota-upload
  (:require [babashka.cli :as cli] [babashka.fs :as fs] [babashka.process :as p]
            [cheshire.core :as json] [clojure.string :as str])
  (:import [java.nio.file Files LinkOption]
           [java.nio.file.attribute PosixFilePermissions]
           [java.security MessageDigest] [java.util Arrays]))
(defn bytes-hex [bytes] (apply str (map #(format "%02x" (bit-and 255 %)) bytes)))
(defn sha [bytes] (.digest (MessageDigest/getInstance "SHA-256") bytes))
(defn quoted [s]
  (when (re-find #"[\r\n\u0000]" (str s)) (throw (ex-info "Invalid curl value" {})))
  (str "\"" (str/escape (str s) {\\ "\\\\" \" "\\\""}) "\""))
(defn private-file [file]
  (let [path (fs/path file) nf (into-array LinkOption [LinkOption/NOFOLLOW_LINKS])]
    (when-not (and (Files/isRegularFile path nf)
                   (= (str (Files/getOwner path nf)) (System/getProperty "user.name"))
                   (= "rw-------" (PosixFilePermissions/toString (Files/getPosixFilePermissions path nf))))
      (throw (ex-info "Expected owner-only0600 regular file" {})))
    (Files/readAllBytes path)))
(defn private-output [file]
  (fs/create-file file {:posix-file-permissions "rw-------"})
  (str file))
(defn curl! [config evidence name args]
  (let [body (private-output (fs/path evidence (str name ".json")))
        log (private-output (fs/path evidence (str name ".curl.log")))
        {:keys [exit out err]} @(p/process (into ["curl" "--config" config "--output" body
                                                  "--write-out" "%{http_code}"] args)
                                           {:out :string :err :string})]
    (spit log (str "exit=" exit " http=" out "\n" err))
    {:ok (and (zero? exit) (= "200" out)) :code out :body body}))
(defn status! [config evidence name]
  (let [r (curl! config evidence name ["--url" "https://wrover.local:8443/status" "--max-time" "8"])]
    (when (:ok r) (try (json/parse-string (slurp (:body r)) true) (catch Exception _ nil)))))
(defn other-slot [address] (case address 65536 1441792 1441792 65536 nil))
(defn main [args]
  (let [{:keys [ip image cert token sha256 slot-address evidence-dir]} (cli/parse-opts args {:coerce {:ip :string :image :string :cert :string
                                                                                                      :token :string :sha256 :string :slot-address :string
                                                                                                      :evidence-dir :string}})
        _ (when-not (and ip image cert token sha256 slot-address evidence-dir
                         (re-matches #"[0-9a-f]{64}" sha256)
                         (re-matches #"[0-9]{1,3}(\.[0-9]{1,3}){3}" ip)
                         (every? #(<= 0 (parse-long %) 255) (str/split ip #"\."))
                         (#{"0x10000" "0x160000"} slot-address))
            (throw (ex-info "Required: --ip --image --cert --token --sha256 --slot-address --evidence-dir" {})))
        bytes (Files/readAllBytes (fs/path image))
        _ (when-not (and (<= 320 (alength bytes) 1376256) (= sha256 (bytes-hex (sha bytes))))
            (throw (ex-info "Image size/hash differs from explicit approval" {})))
        footer (Arrays/copyOfRange bytes (- (alength bytes) 32) (alength bytes))
        _ (when-not (Arrays/equals footer (sha (Arrays/copyOfRange bytes 0 (- (alength bytes) 32))))
            (throw (ex-info "Invalid appended image SHA-256" {})))
        expected (bytes-hex footer)
        secret (private-file token)
        _ (when-not (= 32 (alength secret)) (throw (ex-info "Token must be32bytes" {})))
        _ (private-file cert)
        target (Long/parseLong (subs slot-address 2) 16)
        _ (when (fs/exists? evidence-dir) (throw (ex-info "Evidence path exists; no retry" {})))
        _ (fs/create-dirs evidence-dir {:posix-file-permissions "rwx------"})
        config (private-output (fs/path evidence-dir "request.curl"))]
    (try
      (spit config
            (str "silent\nshow-error\nhttp1.1\nretry = 0\nconnect-timeout = 8\n"
                 "noproxy = \"*\"\nproto = \"=https\"\n"
                 "connect-to = " (quoted (str "wrover.local:8443:" ip ":8443")) "\n"
                 "cacert = " (quoted (fs/absolutize cert)) "\n"
                 "header = " (quoted (str "Authorization: Bearer " (bytes-hex secret))) "\n"))
      (let [before (status! config evidence-dir "before")]
        (when-not (and (true? (:online before)) (true? (:healthy before))
                       (true? (:confirmed before)) (= target (other-slot (:slot_address before))))
          (throw (ex-info "Device not healthy/confirmed or target not inactive" {})))
        (let [r (curl! config evidence-dir "upload"
                       ["--url" "https://wrover.local:8443/ota" "--max-time" "125"
                        "--request" "POST" "--header" "Content-Type: application/octet-stream"
                        "--header" (str "X-Image-SHA256: " sha256)
                        "--header" (str "X-OTA-Target: " slot-address) "--header" "Expect:"
                        "--data-binary" (str "@" (fs/absolutize image))])]
          (when (str/starts-with? (:code r) "4")
            (throw (ex-info "Upload rejected; no automatic retry" {}))))
        ;; Lost ACK may mean successful reboot. Poll GET only; NEVER repeat POST.
        (let [deadline (+ (System/nanoTime) 180000000000)]
          (loop [i 0]
            (when (>= (System/nanoTime) deadline)
              (throw (ex-info "Target boot/confirmation not verified; no automatic retry" {})))
            (Thread/sleep 2000)
            (let [s (status! config evidence-dir (str "after-" i))]
              (if (and (= target (:slot_address s)) (= expected (:image_sha256 s))
                       (true? (:online s)) (true? (:healthy s)) (true? (:confirmed s)))
                (do (spit (private-output (fs/path evidence-dir "result.json"))
                          (json/generate-string {:pass true :sha256 sha256 :target slot-address
                                                 :version (:version s) :uploaded-once true}))
                    (println "Wi-Fi OTA verified: image/slot, online, healthy, boot confirmed."))
                (recur (inc i)))))))
      (finally (fs/delete-if-exists config)))))
(when (seq *command-line-args*) (main *command-line-args*))
