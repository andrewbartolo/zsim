#include "bridge.h"

Bridge::Bridge(uint32_t lineSize, g_string& name) : lineSize(lineSize),
    name(name)
{
    printf("bridge constructor called\n");
}

Bridge::~Bridge()
{
    printf("bridge destructor called\n");
}


/*
 * If this were a traditional zsim function, it would return a uint64_t
 * indicating the current cycle upon completion of the access. However, the
 * receiver simulator process keeps track of all its own stats separately
 * (though it does use the zsim start cycle as input). We keep the signatures
 * the same and just return the input cycle.
 */
uint64_t
Bridge::access(MemReq& req)
{
    /*
     * NOTE: still need to dereference + set *req.state, because other places in
     * the zsim codebase expect it.
     */
    switch (req.type) {
        case PUTX:
            //Dirty wback
            //Note no break
        case PUTS:
            //Not a real access -- memory must treat clean wbacks as if they never happened.
            *req.state = I;
            break;
        case GETS:
            *req.state = req.is(MemReq::NOEXCL)? S : E;
            break;
        case GETX:
            *req.state = M;
            break;
        default: panic("!?");
    }

    /*
     * Now that we've patched up the correct zsim state, actually send a packet
     * to the receiver process.
     * TODO
     */

    return req.cycle + 0;
}
