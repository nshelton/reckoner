#include "BackendFactory.h"
#include "FakeBackend.h"
#include "HttpBackend.h"

void BackendSet::cancelAll()
{
    if (gps)             gps->cancelFetch();
    if (photo)           photo->cancelFetch();
    if (calendar)        calendar->cancelFetch();
    if (googleTimeline)  googleTimeline->cancelFetch();
}

Backend* BackendSet::byIndex(int i)
{
    switch (i) {
        case 0: return gps.get();
        case 1: return photo.get();
        case 2: return calendar.get();
        case 3: return googleTimeline.get();
        default: return nullptr;
    }
}

BackendSet createBackends(const BackendConfig& config, const std::string& url)
{
    BackendSet set;

    if (config.type == BackendConfig::Type::Fake) {
        set.gps = std::make_unique<FakeBackend>(1000);
    } else {
        set.gps             = std::make_unique<HttpBackend>(url, "location.gps");
        set.photo           = std::make_unique<HttpBackend>(url, "photo");
        set.calendar        = std::make_unique<HttpBackend>(url, "calendar.event");
        set.googleTimeline  = std::make_unique<HttpBackend>(url, "location.googletimeline");
    }

    return set;
}
