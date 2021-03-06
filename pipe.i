/* This is the pipeline, organized as a series of calls to the macro prog.
 * the macro is defined in different ways in the source code to expand to
 * the different data structures necessary to have the different programs
 * and the lists of command line arguments.  You will not need much help
 * to configure your own pipeline */
prog(DISK_DUMP, "dd", "if=/dev/urandom", "ibs=1024")
prog(BASE64, "uuencode", "-")
prog(HEAD, "head", "-n", "20")
prog(PR, "pr")
#if 0
prog(PAGER, "less", "-RF")
#endif
