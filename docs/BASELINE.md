# Build / runtime baseline

## Vulkan

```text
Block layer build: PASS
Artifact: build-win/src/hooks/block.dll
Vulkan runtime (vkcube): PASS
```

Env: `ENABLE_BLOCK=1`, `VK_LAYER_PATH=…\build-win\runtime\vulkan`, layer `VK_LAYER_BLOCK_post_processing_64`.

## OpenGL

```text
Hook build: PASS
Artifacts: build-win/hook/opengl32.dll, Block64.dll
Firestorm load: PASS
```

Injector version ships as 6.8.x so `__RESHADE__` satisfies modern effect headers. Overlay: Shift+B. Brand Signal lime in StyleIndex=2.

## Dual-backend

| Backend | Build | Runtime |
|---------|-------|---------|
| Vulkan | PASS | PASS (vkcube) |
| OpenGL | PASS | PASS (Firestorm load) |
