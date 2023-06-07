/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "sharable_user_data.h"

class MODUserData : public SpikeModule {
public:
	static void servers(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(SharableUserData);
		}
	}
};


IMPL_SPIKE_MODULE(user_data, MODUserData)
