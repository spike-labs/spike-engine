/**
 * resource_recognizer.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/object/ref_counted.h"

//ResourceFormatRecognizer
class ResourceFormatRecognizer : public RefCounted {
    GDCLASS(ResourceFormatRecognizer, RefCounted);

public:
    ResourceFormatRecognizer() { }

    virtual bool recognize(const String &p_path) const = 0;
    virtual void open_editor(const String &p_path) = 0;
};

//ResourceRecognizer
class ResourceRecognizer : public Object{
    GDCLASS(ResourceRecognizer, Object);

private:
    static ResourceRecognizer *singleton;

public:
    static ResourceRecognizer *get_singleton() {
        if(nullptr == singleton) {
            singleton = memnew(ResourceRecognizer);
        }
        return singleton;
    }

private:
    Vector<Ref<ResourceFormatRecognizer>> recognizers;

public:
    ResourceRecognizer() { }

    void add_recongizer(const Ref<ResourceFormatRecognizer> &p_recognizer, bool p_first_priority = false);
    void remove_recongizer(const Ref<ResourceFormatRecognizer> &p_recognizer);

    Ref<ResourceFormatRecognizer> recognize(const String &p_path) const;
};