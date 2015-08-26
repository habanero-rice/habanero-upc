namespace hupcpp {
static __inline__ int hcpp_atomic_inc(volatile int *ptr) {
	unsigned char c;
	__asm__ __volatile__(

			"lock       ;\n"
			"incl %0; sete %1"
			: "+m" (*(ptr)), "=qm" (c)
			  : : "memory"
	);
	return c!= 0;
}

static __inline__ int hcpp_atomic_dec(volatile int *ptr) {
	unsigned char rt;
	__asm__ __volatile__(
			"lock;\n"
			"decl %0; sete %1"
			: "+m" (*(ptr)), "=qm" (rt)
			  : : "memory"
	);
	return rt != 0;
}

#define HC_CACHE_LINE 64

static __inline__ void hc_mfence() {
	__asm__ __volatile__("mfence":: : "memory");
}

/*
 * if (*ptr == ag) { *ptr = x, return 1 }
 * else return 0;
 */
static __inline__ int hc_cas(volatile int *ptr, int ag, int x) {
	int tmp;
	__asm__ __volatile__("lock;\n"
			"cmpxchgl %1,%3"
			: "=a" (tmp) /* %0 EAX, return value */
			  : "r"(x), /* %1 reg, new value */
			    "0" (ag), /* %2 EAX, compare value */
			    "m" (*(ptr)) /* %3 mem, destination operand */
			    : "memory" /*, "cc" content changed, memory and cond register */
	);
	return tmp == ag;
}

}
