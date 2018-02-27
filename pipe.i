prog(HD,"od", "-xc", "/dev/urandom")
prog(HEAD,"head")
prog(SED,"sed", "s/.*:\\(.*\\):.*/[\\1]/")
prog(SORT,"sort", "-r")
