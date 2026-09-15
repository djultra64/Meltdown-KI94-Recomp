// Seeds only addresses supported by MAME captures for Killer Instinct v1.5d.
//@category KillerInstinct

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class SeedKi15d extends GhidraScript {

    private Address address(String value) throws Exception {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void seedCode(String value, String name, boolean function, boolean entry)
            throws Exception {
        Address target = address(value);
        if (!currentProgram.getMemory().contains(target)) {
            println("Skipping " + name + ": address not in this program");
            return;
        }
        disassemble(target);
        createLabel(target, name, true, SourceType.USER_DEFINED);
        if (entry) {
            currentProgram.getSymbolTable().addExternalEntryPoint(target);
        }
        if (function) {
            Function existing = getFunctionAt(target);
            if (existing == null) {
                existing = createFunction(target, name);
            }
            if (existing != null) {
                existing.setName(name, SourceType.USER_DEFINED);
            }
        }
        println("Seeded " + name + " at " + target);
    }

    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            throw new IllegalStateException("No program is open");
        }
        String language = currentProgram.getLanguageID().toString();
        if (!language.equals("MIPS:LE:64:64-32addr")) {
            throw new IllegalStateException("Unexpected language: " + language);
        }

        seedCode("bfc00000", "boot_reset_vector", false, true);
        seedCode("bfc00388", "boot_entry", true, true);

        seedCode("88000000", "loaded_vector", false, true);
        seedCode("880001b8", "loaded_entry", true, true);
        seedCode("8800034c", "jump_to_main_controller", false, false);
        seedCode("88001b90", "prepare_secondary_type1a_render", true, false);
        seedCode("88004180", "update_planar_motion", true, false);
        seedCode("880053f4", "allocate_secondary_object_slot", true, false);
        seedCode("880054d0", "clear_object_record", true, false);
        seedCode("880063ac", "initialize_object_animation", true, false);
        seedCode("88006670", "common_animation_interpreter", true, false);
        seedCode("8800700c", "copy_record_fields", true, false);
        seedCode("88002bb4", "spawn_type_1f_from_fighter", false, false);
        seedCode("88003d30", "emit_counted_type_1a", false, false);
        seedCode("88004408", "transition_type_14_to_15", false, false);
        seedCode("88004570", "update_type_12", false, false);
        seedCode("880045bc", "transition_type_13_to_12", false, false);
        seedCode("88004e54", "update_type_1a_particle", true, false);
        seedCode("88006d90", "script_opcode_15_handler", false, false);
        seedCode("88007cdc", "script_opcode_48_handler", false, false);
        seedCode("88007ff8", "script_opcode_51_handler", false, false);
        seedCode("8800842c", "update_scaled_vertical_motion", true, false);
        seedCode("8800a52c", "create_type_14_on_hit", false, false);
        seedCode("8800b1fc", "create_type_1a_from_fighter", true, false);
        seedCode("88012614", "wait_vblank_cycle", true, false);
        seedCode("8802aa24", "main_controller", true, true);
        seedCode("8802ae14", "main_loop_head", false, false);
        seedCode("8802d5b0", "checksum_mix_step", true, false);
        seedCode("8802d5e0", "checksum_mix_step_x3", true, false);
    }
}
