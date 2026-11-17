#pragma once
namespace VrExternal {
// Release the external queue on every return path, including a failed Submit.
// No D3D commands may be emitted inside a guard that owns the Vulkan queue.
template<class Api,bool Textures> class Guard {
    Api* api;
public:
    explicit Guard(Api* value):api(value) {
        if(!api)return;
        const auto result=Textures?api->BeginTextureSubmission():api->LockVulkanQueue();
        if(result!=0)api=nullptr;
    }
    ~Guard(){if(api){if constexpr(Textures)api->EndTextureSubmission();else api->UnlockVulkanQueue();}}
    explicit operator bool() const {return api!=nullptr;}
    Guard(const Guard&)=delete;
    Guard& operator=(const Guard&)=delete;
};
}
