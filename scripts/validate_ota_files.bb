#!/usr/bin/env bb
(require '[clojure.string :as str])
(import '[java.nio.file Files Paths LinkOption] '[java.nio.file.attribute PosixFilePermission])
(defn check-file [file]
  (let [path (Paths/get file (make-array String 0))
        no-follow (into-array LinkOption [LinkOption/NOFOLLOW_LINKS])]
    (when-not (and (Files/isRegularFile path no-follow)
                   (= (str (Files/getOwner path no-follow)) (System/getProperty "user.name"))
                   (= #{PosixFilePermission/OWNER_READ PosixFilePermission/OWNER_WRITE}
                      (set (Files/getPosixFilePermissions path no-follow))))
      (throw (ex-info "OTA input must be an owner-only0600 regular file" {})))
    (Files/readAllBytes path)))
(when (seq *command-line-args*)
  (let [[token cert key] (mapv check-file *command-line-args*)]
    (when-not (and (= 3 (count *command-line-args*)) (= 32 (alength token))
                   (str/includes? (String. cert "US-ASCII") "BEGIN CERTIFICATE")
                   (str/includes? (String. key "US-ASCII") "PRIVATE KEY"))
      (throw (ex-info "Invalid OTA inputs" {})))))
