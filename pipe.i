prog(DISK_DUMP, "dd", "if=/dev/urandom", "ibs=1024")
prog(BASE64, "uuencode", "-")
/*prog(HEAD, "head", "-n", "40")*/
prog(PR, "pr")
prog(PAGER, "less", "-RF")
