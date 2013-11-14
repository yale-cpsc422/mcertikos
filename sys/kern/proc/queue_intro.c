#define NUM_PROC	64
#define NUM_CHAN	64

struct TDQ {
	unsigned int head;
	unsigned int tail;
};

struct TDQ TDQPool_LOC[NUM_CHAN + 1];

void
tdq_init(unsigned int chid)
{
	TDQPool_LOC[chid].head = NUM_PROC;
	TDQPool_LOC[chid].tail = NUM_PROC;
}

unsigned int
tdq_get_head(unsigned int chid)
{
	return TDQPool_LOC[chid].head;
}

void
tdq_set_head(unsigned int chid, unsigned int head)
{
	TDQPool_LOC[chid].head = head;
}

unsigned int
tdq_get_tail(unsigned int chid)
{
	return TDQPool_LOC[chid].tail;
}

void
tdq_set_tail(unsigned int chid, unsigned int tail)
{
	TDQPool_LOC[chid].tail = tail;
}
