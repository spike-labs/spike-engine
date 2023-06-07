/**
 * resource_recognizer.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "resource_recognizer.h"

ResourceRecognizer *ResourceRecognizer::singleton = nullptr;

void ResourceRecognizer::add_recongizer(const Ref<ResourceFormatRecognizer> &p_recognizer, bool p_first_priority) {
    ERR_FAIL_COND(p_recognizer.is_null());
    ERR_FAIL_COND_MSG(recognizers.find(p_recognizer) >= 0, ERR_ALREADY_EXISTS);
    if(p_first_priority) {
        recognizers.insert(0, p_recognizer);
    } else {
        recognizers.append(p_recognizer);
    }
}

void ResourceRecognizer::remove_recongizer(const Ref<ResourceFormatRecognizer> &p_recognizer) {
    ERR_FAIL_COND_MSG(recognizers.find(p_recognizer) < 0, ERR_DOES_NOT_EXIST);
    recognizers.erase(p_recognizer);
}

Ref<ResourceFormatRecognizer> ResourceRecognizer::recognize(const String &p_path) const {
    for (int i = 0; i < recognizers.size(); i++) {
        if(recognizers[i]->recognize(p_path)) {
            return recognizers[i];
        }
    }
    return nullptr;
}