#ifndef RDMA_MESSAGES_H
#define RDMA_MESSAGES_H

const size_t BUFFER_SIZE = 10 * 1024 * 1024;

enum message_id
    {
        MSG_INVALID = 0,
        MSG_MR,
        MSG_READY,
        MSG_DONE
    };

struct message
{
    int id;

    union
    {
        struct
        {
            uint64_t addr;
            uint32_t rkey;
            uint64_t total_edges;
        } mr;
    } data;
};

#endif
