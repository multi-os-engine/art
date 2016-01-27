    /*
     * Throw an exception object in the current thread.
     */
    /* throw vAA */
    EXPORT_PC
    lsr      w2, wINST, #8               // r2<- AA
    GET_VREG w1, w2                      // r1<- vAA (exception object)
    cbz      w0, common_errNullObject
    str      w1, [xSELF, #THREAD_EXCEPTION_OFFSET]  // thread->exception<- obj
    b        MterpException
