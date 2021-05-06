#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <stdint.h>
#include <windows.h>
#include <wrl.h>

namespace grafix {

using namespace Microsoft::WRL;

namespace state {

  constexpr uint32_t frameCount = 2;
  extern uint32_t    frameIndex;

  extern HWND                              hwnd;
  extern ComPtr<ID3D12Device>              device;
  extern ComPtr<ID3D12CommandQueue>        commandQueue;
  extern ComPtr<ID3D12CommandAllocator>    commandAllocator;
  extern ComPtr<ID3D12GraphicsCommandList> commandList;
  extern ComPtr<ID3D12PipelineState>       pipelineState;
  extern ComPtr<IDXGISwapChain3>           swapchain;
  extern ComPtr<ID3D12DescriptorHeap>      rtvHeap;
  extern uint32_t                          rtvDescriptorSize;
  extern ComPtr<ID3D12Resource>            renderTargets[frameCount];
  extern ComPtr<ID3D12Fence>               fence;
  extern HANDLE                            fenceEvent;
  extern uint64_t fenceValue; // Unsure why this is 64 bits

} // namespace state

void CreateHwnd(HINSTANCE hInstance);
void GetHardwareAdapter(IDXGIFactory1 *factory, IDXGIAdapter1 **retAdapter);
void LoadPipeline(void);
void LoadAssets(void);
void WaitForPreviousFrame(void);
void Terminate(void);
void Render(void);

struct CPU_DESCRIPTOR_HANDLE : public D3D12_CPU_DESCRIPTOR_HANDLE {
  CPU_DESCRIPTOR_HANDLE() = default;
};

} // namespace grafix
