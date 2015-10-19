/*
 * forasyncAny1D.h
 *  
 *      Author: Vivek Kumar (vivekk@rice.edu)
 */

#ifndef FORASYNCANY1D_H_
#define FORASYNCANY1D_H_

template <typename T>
inline void forasyncAny1D_runner(hupcpp::loop_domain_t* loop, T lambda) {
	hupcpp::loop_domain_t loop0 = loop[0];
	for(int i=loop0.low; i<loop0.high; i+=loop0.stride) {
		lambda(i);
	}
}

template <typename T>
inline void forasyncAny1D_recursive(hupcpp::loop_domain_t* loop, T lambda) {
	int low=loop->low, high=loop->high, stride=loop->stride, tile=loop->tile;
	//split the range into two, spawn a new task for the first half and recurse on the rest
	if((high-low) > tile) {
		int mid = (high+low)/2;
		// upper-half
		// delegate scheduling to the underlying runtime
		hupcpp::asyncAny([=]() {
			hupcpp::loop_domain_t ld = {mid, high, stride, tile};
			forasyncAny1D_recursive<T>(&ld, lambda);
		});
		// update lower-half
		//continue to work on the half task
		hupcpp::loop_domain_t ld = {low, mid, stride, tile};
		forasyncAny1D_recursive<T>(&ld, lambda);
	} else {
		//compute the tile
		hupcpp::loop_domain_t ld = {low, high, stride, tile};
		forasyncAny1D_runner<T>(&ld, lambda);
	}
}

template <typename T>
inline void forasyncAny1D(hupcpp::loop_domain_t* loop, T lambda) {
		forasyncAny1D_recursive<T>(loop, lambda);
}

#endif /* FORASYNCANY1D_H_ */
