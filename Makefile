PYTHON ?= python3
CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2

.PHONY: check test-python test-native packed-frame-tool pc-demo sdl-smoke poc110-compare provenance-check knowledge-check verify-check doctor clean

check: test-python test-native provenance-check knowledge-check verify-check pc-demo

test-python:
	$(PYTHON) -m unittest discover -s tests -v

test-native:
	mkdir -p native/build
	$(CC) $(CFLAGS) -Inative/include native/src/pc_viewport.c native/tests/test_pc_viewport.c -o native/build/test_pc_viewport
	./native/build/test_pc_viewport
	$(CC) $(CFLAGS) -Inative/include native/tests/test_mips_word_ops.c -o native/build/test_mips_word_ops
	./native/build/test_mips_word_ops
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/tests/test_memory.c -o native/build/test_memory
	./native/build/test_memory
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_88004180.c native/tests/test_ki15d_88004180.c -o native/build/test_ki15d_88004180
	./native/build/test_ki15d_88004180
	$(CC) $(CFLAGS) -Inative/include native/src/original/ki15d_8802d5b0.c native/tests/test_ki15d_8802d5b0.c -o native/build/test_ki15d_8802d5b0
	./native/build/test_ki15d_8802d5b0
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_8800700c.c native/tests/test_ki15d_8800700c.c -o native/build/test_ki15d_8800700c
	./native/build/test_ki15d_8800700c
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880053f4.c native/tests/test_ki15d_880053f4.c -o native/build/test_ki15d_880053f4
	./native/build/test_ki15d_880053f4
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880054d0.c native/tests/test_ki15d_880054d0.c -o native/build/test_ki15d_880054d0
	./native/build/test_ki15d_880054d0
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880063ac.c native/tests/test_ki15d_880063ac.c -o native/build/test_ki15d_880063ac
	./native/build/test_ki15d_880063ac
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_88006670_type1a.c native/tests/test_ki15d_88006670_type1a.c -o native/build/test_ki15d_88006670_type1a
	./native/build/test_ki15d_88006670_type1a
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_8800842c.c native/tests/test_ki15d_8800842c.c -o native/build/test_ki15d_8800842c
	./native/build/test_ki15d_8800842c
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880053f4.c native/src/original/ki15d_880063ac.c native/src/original/ki15d_8800b1fc.c native/tests/test_ki15d_8800b1fc.c -o native/build/test_ki15d_8800b1fc
	./native/build/test_ki15d_8800b1fc
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_88003d30.c native/tests/test_ki15d_88003d30_decision.c -o native/build/test_ki15d_88003d30_decision
	./native/build/test_ki15d_88003d30_decision
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880053f4.c native/src/original/ki15d_880063ac.c native/src/original/ki15d_8800b1fc.c native/src/original/ki15d_88003d30.c native/tests/test_ki15d_88003d30.c -o native/build/test_ki15d_88003d30
	./native/build/test_ki15d_88003d30
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_8800a2e4.c native/src/original/ki15d_88003d30.c native/tests/test_ki15d_8800a2e4.c -o native/build/test_ki15d_8800a2e4
	./native/build/test_ki15d_8800a2e4
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_88004180.c native/src/original/ki15d_880053f4.c native/src/original/ki15d_880054d0.c native/src/original/ki15d_880063ac.c native/src/original/ki15d_88006670_type1a.c native/src/original/ki15d_8800842c.c native/src/original/ki15d_8800b1fc.c native/src/original/ki15d_88004e54.c native/tests/test_ki15d_type1a_lifetime.c -o native/build/test_ki15d_type1a_lifetime
	./native/build/test_ki15d_type1a_lifetime
	$(PYTHON) tools/generate_native_pilot.py --check
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880054d0.c native/src/native_pilot.c native/src/native_pilot_snapshot.c native/tests/test_static_region.c -lm -o native/build/test_static_region
	./native/build/test_static_region
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880054d0.c native/src/native_pilot.c native/src/native_pilot_snapshot.c native/tests/test_native_pilot.c -lm -o native/build/test_native_pilot
	./native/build/test_native_pilot
	$(PYTHON) tools/check_connected_phase.py
	$(PYTHON) tools/compare_connected_phase_oracle.py
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/frame_asset.c native/src/original/ki15d_880054d0.c native/src/native_pilot.c native/src/native_pilot_snapshot.c native/tests/test_frame_selection_pilot.c -lm -o native/build/test_frame_selection_pilot
	./native/build/test_frame_selection_pilot
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880054d0.c native/src/native_pilot.c native/src/native_pilot_snapshot.c native/tests/test_contact_exact_fp.c -lm -o native/build/test_contact_exact_fp
	./native/build/test_contact_exact_fp
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/original/ki15d_880054d0.c native/src/native_pilot.c native/src/native_pilot_snapshot.c native/tests/test_render_pilot.c -lm -o native/build/test_render_pilot
	./native/build/test_render_pilot
	$(PYTHON) tools/compare_projection_transaction.py
	$(CC) $(CFLAGS) -Inative/include native/src/packed_frame.c native/tests/test_packed_frame.c -o native/build/test_packed_frame
	./native/build/test_packed_frame
	$(CC) $(CFLAGS) -Inative/include native/src/bgr555.c native/tests/test_bgr555.c -o native/build/test_bgr555
	./native/build/test_bgr555
	$(CC) $(CFLAGS) -Inative/include native/src/bgr555.c native/src/packed_renderer.c native/tests/test_packed_renderer.c -o native/build/test_packed_renderer
	./native/build/test_packed_renderer
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/bgr555.c native/src/packed_renderer.c native/src/original/ki15d_88001b90_type1a.c native/tests/test_ki15d_88001b90_type1a.c -o native/build/test_ki15d_88001b90_type1a
	./native/build/test_ki15d_88001b90_type1a
	$(CC) $(CFLAGS) -Inative/include native/src/endokuken_capture.c native/tests/test_endokuken_capture.c -o native/build/test_endokuken_capture
	./native/build/test_endokuken_capture

packed-frame-tool:
	mkdir -p native/build
	$(CC) $(CFLAGS) -Inative/include native/src/packed_frame.c native/src/bgr555.c native/tools/packed_frame_export.c -o native/build/packed_frame_export

pc-demo:
	mkdir -p native/build
	$(CC) $(CFLAGS) -Inative/include native/src/bgr555.c native/src/packed_renderer.c native/src/endokuken_capture.c native/src/pc_viewport.c native/src/pc_window_sdl2.c native/tools/endokuken_demo.c -ldl -o native/build/endokuken_demo

sdl-smoke:
	mkdir -p native/build
	$(CC) $(CFLAGS) -Inative/include native/src/pc_viewport.c native/src/pc_window_sdl2.c native/tests/test_pc_window_sdl2.c -ldl -o native/build/test_pc_window_sdl2
	SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./native/build/test_pc_window_sdl2

# Developer-only selector for the trace-backed transform oracle versus the
# native guest-state path. Release/demo code does not expose this scaffold.
poc110-compare:
	mkdir -p native/build
	$(CC) $(CFLAGS) -Inative/include native/src/memory.c native/src/bgr555.c native/src/packed_renderer.c native/src/original/ki15d_88001b90_type1a.c native/tests/test_ki15d_88001b90_type1a.c -o native/build/test_ki15d_88001b90_type1a
	./native/build/test_ki15d_88001b90_type1a --render-source compare

provenance-check:
	$(PYTHON) tools/ki_project.py provenance-check provenance/functions

knowledge-check:
	$(PYTHON) tools/knowledge.py check

verify-check:
	$(PYTHON) tools/verify_evidence.py check

doctor:
	$(PYTHON) tools/ki_project.py doctor

clean:
	rm -rf native/build
