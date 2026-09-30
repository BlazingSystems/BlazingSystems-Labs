# Local AI Studio

**Type:** Local Python AI engine and browser interface  
**Stage:** Engine prototype

A local-first experimentation environment for image generation, upscaling, simple motion/video workflows, and optional text-to-speech adapters.

## Architecture

- localhost Python HTTP service;
- browser-based control interface;
- CPU-oriented/low-memory execution path;
- Diffusers-compatible Stable Diffusion adapter;
- optional Real-ESRGAN integration with image-resize fallback;
- still-image motion/interpolation utilities;
- optional Piper TTS adapter.

## Validation

- Python syntax: passed;
- local server startup: passed;
- `GET /api/status`: passed;
- model-dependent generation: requires separately obtained model weights and has not been certified on every target machine.

## Hardware Reality

Very old CPUs and minimal VRAM can run only a limited subset of modern AI workloads at practical speed. The project is intended as a learning and local-processing environment, not as a claim that low-spec hardware matches modern GPU workstations.

Model weights are not stored in this repository. Review each model's license before use.
