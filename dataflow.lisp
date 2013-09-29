;; Dataflow analysis frameworks for the optimizer
;;
;; This should at least perform constant propagation and dead code
;; elimination

(defpackage :dataflow
  (:use :cl :pcf2-bc :setmap)
  (:export make-cfg
           ops-from-cfg
           get-lbls-in-order
           basic-block
           basic-block-ops
           basic-block-preds
           basic-block-succs
           )
  )

(in-package :dataflow)

(defstruct (basic-block
             (:print-function
              (lambda (struct stream depth)
                (declare (ignore depth))
                (format stream "~&Basic block:~%")
                (format stream "Preds: ~A~%" (basic-block-preds struct))
                (format stream "Succs: ~A~%" (basic-block-succs struct))
                )
              )
             )
  (ops)
  (preds)
  (succs)
  (:documentation "This represents a basic block in the control flow graph.")
  )

(defmacro add-op (op bb &body body)
  `(let ((,bb (make-basic-block
               :ops (cons ,op (basic-block-ops ,bb))
               :preds (basic-block-preds ,bb)
               :succs (basic-block-succs ,bb)
               )
           )
         )
     ,@body
     )
  )

(defmacro add-pred (prd bb &body body)
  `(let ((,bb (make-basic-block
               :ops (basic-block-ops ,bb)
               :preds (cons ,prd (basic-block-preds ,bb))
               :succs (basic-block-succs ,bb)
               )
           )
         )
     ,@body
     )
  )

(defmacro add-succ (prd bb &body body)
  `(let ((,bb (make-basic-block
               :ops (basic-block-ops ,bb)
               :preds (basic-block-preds ,bb)
               :succs (cons ,prd (basic-block-succs ,bb))
               )
           )
         )
     ,@body
     )
  )

(defgeneric find-basic-blocks (op blocks curblock blkid)
  (:documentation "Construct a set of basic blocks from a list of opcodes")
  )

;; For most instructions, we do not terminate the block
(defmethod find-basic-blocks ((op instruction) blocks curblock blkid)
  (add-op op curblock
    (list blocks curblock blkid)
    )
  )

(defmethod find-basic-blocks ((op label) blocks curblock blkid)
  (let ((new-block (make-basic-block :ops (list op)))
        )
    (with-slots (str) op
      (add-succ str curblock
        (list (map-insert blkid curblock blocks) new-block str)
        )
      )
    )
  )

(defmethod find-basic-blocks ((op branch) blocks curblock blkid)
  (declare (optimize (debug 3) (speed 0)))
  (let ((new-block (make-basic-block))
        )
    (with-slots (targ) op
      (add-op op curblock
        (add-succ targ curblock
          (add-succ (concatenate 'string "fall" blkid) curblock
            (list (map-insert blkid curblock blocks) new-block (concatenate 'string "fall" blkid))
            )
          )
        )
      )
    )
  )

(defmethod find-basic-blocks ((op call) blocks curblock blkid)
  (let ((new-block (make-basic-block))
        )
    (add-op op curblock
      (add-succ (concatenate 'string "call" blkid) curblock
        (list (map-insert blkid curblock blocks) new-block (concatenate 'string "call" blkid))
        )
      )
    )
  )

(defmethod find-basic-blocks ((op ret) blocks curblock blkid)
  (let ((new-block (make-basic-block))
        )
    (add-op op curblock
      (add-succ (concatenate 'string "ret" blkid) curblock
        (list (map-insert blkid curblock blocks) new-block (concatenate 'string "ret" blkid))
        )
      )
    )
  )

(defun find-preds (blocks)
  (declare (optimize (debug 3) (speed 0)))
  (map-reduce (lambda (st k v)
                (reduce
                 (lambda (st x)
                   (map-insert x
                               (let ((bb (cdr (map-find x st)))
                                     )
                                 (add-pred k bb
                                   bb
                                   )
                                 )
                               st
                               )
                   )
                 (basic-block-succs v)
                 :initial-value st)
                )
              blocks blocks;(map-empty :comp string<)
              )
  )

(defun make-cfg (ops)
  "Return a control-flow graph from a list of PCF ops.  This is a map
of basic block IDs to basic blocks."
  (declare (optimize (debug 3) (speed 0)))
  (let ((cfg (reduce #'(lambda (x y)
                         (declare (optimize (debug 3) (speed 0))
                                  (type instruction y))
                         (apply #'find-basic-blocks (cons y x))
                         )
                     ops :initial-value (list (map-empty :comp string<) (make-basic-block) ""))
          )
        )
    (let ((cfg (map-insert (third cfg) (second cfg) (first cfg)))
          )
      
      (find-preds
       cfg
       )
      )
    )
  )

(defun get-lbls-in-order (ops res &optional (c ""))
  (declare (optimize (debug 3) (speed 0)))
  (if (null ops)
      (reverse res)
      (let ((str (typecase
                     (first ops)
                   (label (with-slots (str) (first ops)
                            str))
                   (branch (concatenate 'string "fall" c))
                   (ret (concatenate 'string "ret" c))
                   (call (concatenate 'string "call" c))
                   (t c)
                   )
              )
            )
        (get-lbls-in-order (rest ops) 
                           (typecase
                               (first ops)
                             (branch
                              (cons str res))
                             (label
                              (cons str res))
                             (ret
                              (cons str res))
                             (call
                              (cons str res))
                             (t res))
                           str)
        )
      )
  )

(defun ops-from-cfg (cfg lbls-in-order)
  (declare (optimize (debug 3) (speed 0)))
  (labels ((flatten-ops (lbls)
             (if lbls
                 (append
                  (reverse (basic-block-ops (cdr (map-find (first lbls) cfg))))
                  (flatten-ops (rest lbls))
                  )
                 )
             )
           )
    (flatten-ops lbls-in-order)
    )
  )

