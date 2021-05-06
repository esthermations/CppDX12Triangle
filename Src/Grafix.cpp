#include "Grafix.hpp"

#include <WinUser.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <numeric>
#include <shellapi.h>
#include <windows.h>
#include <wrl.h>

namespace grafix {

namespace state {

  uint32_t frameIndex;

  HWND                              hwnd;
  ComPtr<ID3D12Device>              device;
  ComPtr<ID3D12CommandQueue>        commandQueue;
  ComPtr<ID3D12CommandAllocator>    commandAllocator;
  ComPtr<ID3D12GraphicsCommandList> commandList;
  ComPtr<ID3D12PipelineState>       pipelineState;
  ComPtr<IDXGISwapChain3>           swapchain;
  ComPtr<ID3D12DescriptorHeap>      rtvHeap;
  uint32_t                          rtvDescriptorSize;
  ComPtr<ID3D12Resource>            renderTargets[frameCount];
  ComPtr<ID3D12Fence>               fence;
  HANDLE                            fenceEvent;
  uint64_t                          fenceValue; // Unsure why this is 64 bits

} // namespace state

//------------------------------------------------------------------------------
void CreateHwnd(HINSTANCE hInstance) {

  constexpr auto windowProc =
      [](HWND hwnd, uint32_t message, WPARAM wParam, LPARAM lParam) -> void {
    // TODO: copy here:
    // https://github.com/microsoft/DirectX-Graphics-Samples/blob/cab614fc3b2ac880f1909c40a485423649940b0d/Samples/Desktop/D3D12HelloWorld/src/HelloWindow/Win32Application.cpp#L76
  };

  WNDCLASSEX windowClass =
      {.cbSize      = sizeof(WNDCLASSEX),
       .style       = CS_HREDRAW | CS_VREDRAW,
       .lpfnWndProc = WindowProc}

  state::hwnd = CreateWindow(
      "Grafixxxxxx",
      "Esther's GRAFXI",
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      1280,
      720,
      nullptr,
      nullptr,
      hInstance,
      nullptr);

  assert(IsWindow(state::hwnd));
}

//------------------------------------------------------------------------------
void LoadPipeline(void) {
  uint32_t dxgiFactoryFlags = 0;

  {
    // Debug
    ID3D12Debug *debugController;
    const auto success = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (success) {
      debugController->EnableDebugLayer();
      dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
  }

  ComPtr<IDXGIFactory4> factory;
  CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));

  // Create a hardware device
  {
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);

    const auto result = D3D12CreateDevice(
        hardwareAdapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&state::device));

    assert(SUCCEEDED(result));
  }

  // Create a command queue
  {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type  = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
    };

    const auto result = state::device->CreateCommandQueue(
        &queueDesc, IID_PPV_ARGS(&state::commandQueue));
    assert(SUCCEEDED(result));
  }

  // Create a swap chain
  {
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {
        .Width       = 1280,
        .Height      = 720,
        .Format      = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc  = {.Count = 1},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = state::frameCount,
        .SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };

    ComPtr<IDXGISwapChain1> swapchain;

    HRESULT result;

    result = factory->CreateSwapChainForHwnd(
        state::commandQueue.Get(),
        state::hwnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        &swapchain);

    assert(SUCCEEDED(result));

    result = factory->MakeWindowAssociation(state::hwnd, DXGI_MWA_NO_ALT_ENTER);
    assert(SUCCEEDED(result));

    result = swapchain.As(&state::swapchain);
    assert(SUCCEEDED(result));

    state::frameIndex = state::swapchain->GetCurrentBackBufferIndex();
  }

  // Create descriptor heaps
  {
    // rtv = render target view. TODO: look that up.
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        .Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = state::frameCount,
        .Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };

    const HRESULT result = state::device->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(&state::rtvHeap));
    assert(SUCCEEDED(result));

    state::rtvDescriptorSize = state::device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  }

  // Create "frame resources"
  {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        state::rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Create rtv for each frame
    for (uint32_t n = 0; n < state::frameCount; ++n) {
      HRESULT result = state::swapchain->GetBuffer(
          n, IID_PPV_ARGS(&state::renderTargets[n]));
      assert(SUCCEEDED(result));

      state::device->CreateRenderTargetView(
          state::renderTargets[n].Get(), nullptr, rtvHandle);

      rtvHandle.ptr += state::rtvDescriptorSize; // bump the pointer
    }
  }

  const HRESULT result = state::device->CreateCommandAllocator(
      D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&state::commandAllocator));
  assert(SUCCEEDED(result));
}

//------------------------------------------------------------------------------
void LoadAssets(void) {
  HRESULT result;

  result = state::device->CreateCommandList(
      /* nodeMask = */ 0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      state::commandAllocator.Get(),
      nullptr,
      IID_PPV_ARGS(&state::commandList));
  assert(SUCCEEDED(result));

  // Put commandList into closed state (it starts in Record state)
  result = state::commandList->Close();
  assert(SUCCEEDED(result));

  // Create sync objects
  {
    result = state::device->CreateFence(
        0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&state::fence));
    assert(SUCCEEDED(result));

    state::fenceValue = 1;
    state::fenceEvent = CreateEvent(nullptr, false, false, nullptr);

    if (not state::fenceEvent) {
      assert(SUCCEEDED(HRESULT_FROM_WIN32(GetLastError())));
    }
  }
}

//------------------------------------------------------------------------------
void Render(void) {
  HRESULT result;

  // Populate command list
  {
    result = state::commandAllocator.Reset();
    assert(SUCCEEDED(result));

    result = state::commandList->Reset(
        state::commandAllocator.Get(), state::pipelineState.Get());
    assert(SUCCEEDED(result));

    // Swap buffers
    const D3D12_RESOURCE_BARRIER rendToPresent = {
        .Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource   = state::renderTargets[state::frameIndex].Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
            .StateAfter  = D3D12_RESOURCE_STATE_PRESENT,
        }};

    state::commandList->ResourceBarrier(1, &rendToPresent);

    // Draw a colour into the back buffer
    auto handle = state::rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (state::frameIndex * state::rtvDescriptorSize);

    constexpr float clearColour[] = {0.1f, 0.4f, 0.1f, 1.0f};

    state::commandList->ClearRenderTargetView(handle, clearColour, 0, nullptr);

    // Swap the back buffer to front again
    const D3D12_RESOURCE_BARRIER presentToRend = {
        .Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags      = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource   = state::renderTargets[state::frameIndex].Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
            .StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET,
        }};

    state::commandList->ResourceBarrier(1, &presentToRend);

    result = state::commandList->Close();
    assert(SUCCEEDED(result));
  }

  // Execute command list
  {
    ID3D12CommandList *commandListPointers[] = {state::commandList.Get()};
    state::commandQueue->ExecuteCommandLists(1, commandListPointers);
  }

  // Present the frame
  {
    result = state::swapchain->Present(1, 0);
    assert(SUCCEEDED(result));
  }

  WaitForPreviousFrame();

} // namespace grafix

//------------------------------------------------------------------------------
// Wait for previous frame -- essentially the CPU and GPU trade work and while
// one works, the other waits. This obviously isn't efficient but it's simple.
void WaitForPreviousFrame(void) {
  const auto fence  = state::fenceValue;
  HRESULT    result = state::commandQueue->Signal(state::fence.Get(), fence);
  assert(SUCCEEDED(result));

  ++state::fenceValue;

  if (state::fence->GetCompletedValue() < fence) {
    result = state::fence->SetEventOnCompletion(fence, state::fenceEvent);
    WaitForSingleObject(state::fenceEvent, INFINITE);
  }

  state::frameIndex = state::swapchain->GetCurrentBackBufferIndex();
}

//------------------------------------------------------------------------------
void Terminate(void) {
  WaitForPreviousFrame();
  CloseHandle(state::fenceEvent);
}

//------------------------------------------------------------------------------
void GetHardwareAdapter(IDXGIFactory1 *factory, IDXGIAdapter1 **retAdapter) {
  *retAdapter = nullptr;
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<IDXGIFactory6> factory6;

  if (SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory6)))) {
    uint32_t adapterIndex = 0;
    bool     found        = true;

    while (found) {
      found =
          (DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(
                                       adapterIndex,
                                       DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                       IID_PPV_ARGS(&adapter)));

      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        // Ignore software adapters.
        continue;
      }

      const bool canCreateDevice = SUCCEEDED(D3D12CreateDevice(
          adapter.Get(),
          D3D_FEATURE_LEVEL_11_0,
          _uuidof(ID3D12Device),
          nullptr));

      if (canCreateDevice) {
        // We found our boy :)
        break;
      }
    }
  }

  *retAdapter = adapter.Detach();
}

} // namespace grafix
