/**
 * utilities.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#ifdef GLES3_ENABLED

#include "servers/rendering/storage/utilities.h"

#include "platform_config.h"
#ifndef OPENGL_INCLUDE_H
#include <GLES3/gl3.h>
#else
#include OPENGL_INCLUDE_H
#endif

namespace OPENGL3 {

class Utilities : public RendererUtilities {
private:
	static Utilities *singleton;

public:
	static Utilities *get_singleton() { return singleton; }

	Utilities();
	~Utilities();

	// Buffer size is specified in bytes
	static Vector<uint8_t> buffer_get_data(GLenum p_target, GLuint p_buffer, uint32_t p_buffer_size);

	/* INSTANCES */

	virtual RS::InstanceType get_base_type(RID p_rid) const override;
	virtual bool free(RID p_rid) override;

	/* DEPENDENCIES */

	virtual void base_update_dependency(RID p_base, DependencyTracker *p_instance) override;

	/* VISIBILITY NOTIFIER */
	virtual RID visibility_notifier_allocate() override;
	virtual void visibility_notifier_initialize(RID p_notifier) override;
	virtual void visibility_notifier_free(RID p_notifier) override;

	virtual void visibility_notifier_set_aabb(RID p_notifier, const AABB &p_aabb) override;
	virtual void visibility_notifier_set_callbacks(RID p_notifier, const Callable &p_enter_callbable, const Callable &p_exit_callable) override;

	virtual AABB visibility_notifier_get_aabb(RID p_notifier) const override;
	virtual void visibility_notifier_call(RID p_notifier, bool p_enter, bool p_deferred) override;

	/* TIMING */

#define MAX_QUERIES 256
#define FRAME_COUNT 3

	struct Frame {
		GLuint queries[MAX_QUERIES];
		TightLocalVector<String> timestamp_names;
		TightLocalVector<uint64_t> timestamp_cpu_values;
		uint32_t timestamp_count = 0;
		TightLocalVector<String> timestamp_result_names;
		TightLocalVector<uint64_t> timestamp_cpu_result_values;
		TightLocalVector<uint64_t> timestamp_result_values;
		uint32_t timestamp_result_count = 0;
		uint64_t index = 0;
	};

	const uint32_t max_timestamp_query_elements = MAX_QUERIES;

	Frame frames[FRAME_COUNT]; // Frames for capturing timestamps. We use 3 so we don't need to wait for commands to complete
	uint32_t frame = 0;

	virtual void capture_timestamps_begin() override;
	virtual void capture_timestamp(const String &p_name) override;
	virtual uint32_t get_captured_timestamps_count() const override;
	virtual uint64_t get_captured_timestamps_frame() const override;
	virtual uint64_t get_captured_timestamp_gpu_time(uint32_t p_index) const override;
	virtual uint64_t get_captured_timestamp_cpu_time(uint32_t p_index) const override;
	virtual String get_captured_timestamp_name(uint32_t p_index) const override;
	void _capture_timestamps_begin();
	void capture_timestamps_end();

	/* MISC */

	virtual void update_dirty_resources() override;
	virtual void set_debug_generate_wireframes(bool p_generate) override;

	virtual bool has_os_feature(const String &p_feature) const override;

	virtual void update_memory_info() override;

	virtual uint64_t get_rendering_info(RS::RenderingInfo p_info) override;
	virtual String get_video_adapter_name() const override;
	virtual String get_video_adapter_vendor() const override;
	virtual RenderingDevice::DeviceType get_video_adapter_type() const override;
	virtual String get_video_adapter_api_version() const override;

	virtual Size2i get_maximum_viewport_size() const override;
};

} // namespace GLES3

#endif // GLES3_ENABLED
