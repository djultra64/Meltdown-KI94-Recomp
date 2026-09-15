#include "ki/native_pilot.h"
#include "ki/mips_word_ops.h"

#include "ki/original/ki15d_880054d0.h"

#include <limits.h>
#include <fenv.h>
#include <float.h>
#include <string.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 &&
               sizeof(float) == sizeof(uint32_t),
               "projection requires IEC 60559 binary32-compatible float");

typedef struct KiPilotSourceWord { uint32_t pc, word; } KiPilotSourceWord;
typedef struct KiPilotSourceByte { uint32_t address; uint8_t value; } KiPilotSourceByte;

static uint64_t sign32(uint32_t value)
{
    return ki_mips_sign_extend_word(value);
}

static uint32_t arithmetic_shift_right32(uint32_t value, unsigned int shift)
{
    if (shift == 0) return value;
    return (value >> shift) |
           ((value & UINT32_C(0x80000000)) ? (~UINT32_C(0) << (32u - shift)) : 0);
}

static bool signed64_less(uint64_t left, uint64_t right)
{
    const bool left_negative = (left >> 63) != 0;
    const bool right_negative = (right >> 63) != 0;
    return left_negative != right_negative ? left_negative : left < right;
}

static void set_gpr(KiNativePilot *pilot, unsigned int index, uint64_t value)
{
    if (index != 0) pilot->cpu.gpr[index] = value;
}

static bool pilot_stop(KiNativePilot *pilot, KiPilotStatus status,
                       uint64_t pc, uint64_t detail)
{
    pilot->status = status;
    pilot->stop_pc = pc;
    pilot->stop_detail = detail;
    return false;
}

static bool consume_service(KiNativePilot *pilot, uint32_t kind, uint32_t address,
                            uint32_t *value)
{
    /* A mismatch stops before changing the original instruction's destination. */
    if ((pilot->transaction_kind != KI_PILOT_TRANSACTION_CONTACT &&
         pilot->transaction_kind != KI_PILOT_TRANSACTION_CONNECTED) ||
        pilot->service_input_cursor >= pilot->service_input_count)
        return pilot_stop(pilot, KI_PILOT_STOP_SERVICE_INPUT, pilot->cpu.pc, address);
    const KiPilotServiceInput *input =
        &pilot->service_inputs[pilot->service_input_cursor];
    if (input->kind != kind || input->pc != (uint32_t)pilot->cpu.pc ||
        input->address != address)
        return pilot_stop(pilot, KI_PILOT_STOP_SERVICE_INPUT, pilot->cpu.pc, address);
    *value = input->value;
    ++pilot->service_input_cursor;
    return true;
}

static bool emit_service(KiNativePilot *pilot, uint32_t address, uint8_t value)
{
    /* Journal append precedes completion; no external device is called here. */
    if (pilot->service_output_count >= pilot->service_output_capacity)
        return pilot_stop(pilot, KI_PILOT_STOP_SERVICE_OUTPUT, pilot->cpu.pc, address);
    KiPilotServiceOutput *event =
        &pilot->service_outputs[pilot->service_output_count++];
    event->invocation = pilot->invocation_sequence;
    event->pc = (uint32_t)pilot->cpu.pc;
    event->address = address;
    event->value = value;
    return true;
}

static bool aligned(KiNativePilot *pilot, uint64_t address, unsigned int width)
{
    return ((uint32_t)address & (width - 1u)) == 0 ||
           pilot_stop(pilot, KI_PILOT_STOP_UNALIGNED, pilot->cpu.pc, address);
}

static bool read_u8(KiNativePilot *pilot, uint64_t address, uint8_t *value)
{
    if ((uint32_t)address == UINT32_C(0xb0000090)) {
        uint32_t supplied;
        if (!consume_service(pilot, KI_PILOT_SERVICE_MMIO_BYTE,
                             (uint32_t)address, &supplied)) return false;
        *value = (uint8_t)supplied;
        return true;
    }
    return ki_memory_read_u8(&pilot->memory, address, value) == KI_MEMORY_OK ||
           pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address);
}

static bool read_u16(KiNativePilot *pilot, uint64_t address, uint16_t *value)
{
    return aligned(pilot, address, 2) &&
           (ki_memory_read_u16_le(&pilot->memory, address, value) == KI_MEMORY_OK ||
            pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address));
}

static bool read_u32(KiNativePilot *pilot, uint64_t address, uint32_t *value)
{
    return aligned(pilot, address, 4) &&
           (ki_memory_read_u32_le(&pilot->memory, address, value) == KI_MEMORY_OK ||
            pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address));
}

static bool read_u64(KiNativePilot *pilot, uint64_t address, uint64_t *value)
{
    uint32_t low, high;
    if (!aligned(pilot,address,8) || !read_u32(pilot,address,&low) ||
        !read_u32(pilot,address+4u,&high)) return false;
    *value=(uint64_t)low|((uint64_t)high<<32);return true;
}

static bool write_u8(KiNativePilot *pilot, uint64_t address, uint8_t value)
{
    if ((uint32_t)address == UINT32_C(0xb0000090) ||
        (uint32_t)address == UINT32_C(0xb0000098))
        return emit_service(pilot, (uint32_t)address, value);
    return ki_memory_write_u8(&pilot->memory, address, value) == KI_MEMORY_OK ||
           pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address);
}

bool ki_native_pilot_exact_i32_to_f32(int32_t value, uint32_t *output)
{
    uint32_t magnitude = value < 0 ?
        (uint32_t)(-(int64_t)value) : (uint32_t)value;
    if (!output) return false;
    if (!magnitude) { *output = 0; return true; }
    unsigned top = 31;
    while (((magnitude >> top) & 1u) == 0) --top;
    /* A discarded one bit would make the conversion rounding-mode dependent. */
    if (top > 23 && (magnitude & ((UINT32_C(1) << (top - 23)) - 1u)))
        return false;
    uint32_t fraction = (top <= 23 ? magnitude << (23 - top) :
                         magnitude >> (top - 23)) & UINT32_C(0x7fffff);
    *output = (value < 0 ? UINT32_C(0x80000000) : 0u) |
              ((top + 127u) << 23) | fraction;
    return true;
}

static uint32_t integer_sqrt(uint32_t value)
{
    uint32_t root = 0, bit = UINT32_C(1) << 30;
    while (bit > value) bit >>= 2;
    while (bit) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

bool ki_native_pilot_exact_f32_sqrt(uint32_t raw, uint32_t *output)
{
    const uint32_t exponent = (raw >> 23) & 0xffu;
    if (!output || (raw >> 31) || exponent == 0 || exponent == 0xff) return false;
    uint32_t mantissa = UINT32_C(0x800000) | (raw & UINT32_C(0x7fffff));
    int power = (int)exponent - 150;
    if (power & 1) { mantissa <<= 1; --power; }
    const uint32_t root = integer_sqrt(mantissa);
    if (root * root != mantissa) return false;
    int root_top = 31;
    while (((root >> root_top) & 1u) == 0) --root_top;
    const int output_exponent = power / 2 + root_top + 127;
    const uint32_t fraction =
        (root << (23 - root_top)) & UINT32_C(0x7fffff);
    *output = (uint32_t)(output_exponent << 23) | fraction;
    return true;
}

bool ki_native_pilot_exact_f32_to_i32(uint32_t raw, int32_t *output)
{
    uint32_t exponent = (raw >> 23) & 0xffu;
    uint32_t mantissa = UINT32_C(0x800000) | (raw & UINT32_C(0x7fffff));
    if (!output || exponent == 0 || exponent == 0xff) return false;
    int shift = (int)exponent - 150;
    uint64_t magnitude;
    if (shift >= 0) {
        if (shift > 31) return false;
        magnitude = (uint64_t)mantissa << shift;
    } else {
        if (-shift >= 32 ||
            (mantissa & ((UINT32_C(1) << (-shift)) - 1u))) return false;
        magnitude = mantissa >> (-shift);
    }
    const int64_t value = (raw >> 31) ? -(int64_t)magnitude : (int64_t)magnitude;
    if (value < INT32_MIN || value > INT32_MAX) return false;
    *output = (int32_t)value;
    return true;
}

bool ki_native_pilot_sub_word(int32_t left, int32_t right, int32_t *output)
{
    const int64_t result=(int64_t)left-right;
    if(!output||result<INT32_MIN||result>INT32_MAX)return false;
    *output=(int32_t)result;return true;
}

static bool exact_int_to_float(KiNativePilot *pilot, unsigned fd, uint32_t raw)
{
    uint32_t output;
    if (!ki_native_pilot_exact_i32_to_f32((int32_t)raw, &output))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = output;
    return true;
}

static bool exact_float_sqrt(KiNativePilot *pilot, unsigned fd, unsigned fs)
{
    const uint32_t raw = (uint32_t)pilot->cpu.fpr[fs];
    uint32_t output;
    if (!ki_native_pilot_exact_f32_sqrt(raw, &output))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = output;
    return true;
}

static bool exact_float_to_int(KiNativePilot *pilot, unsigned fd, unsigned fs)
{
    const uint32_t raw = (uint32_t)pilot->cpu.fpr[fs];
    int32_t output;
    if (!ki_native_pilot_exact_f32_to_i32(raw, &output))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = (uint32_t)output;
    return true;
}

static bool projection_float_value(uint32_t raw)
{
    const uint32_t exponent = (raw >> 23) & 0xffu;
    return exponent != 0xffu &&
           (exponent != 0 || (raw & UINT32_C(0x7fffff)) == 0);
}

static bool projection_fp_enter(KiNativePilot *pilot, fenv_t *saved)
{
    /* Hold masks traps and clears flags; fesetenv later restores the caller's
     * complete environment without merging guest arithmetic exceptions. */
    fenv_t held;
    if (fegetenv(saved) != 0)
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, 0);
    if (feholdexcept(&held) != 0 || fesetround(FE_TONEAREST) != 0) {
        (void)fesetenv(saved);
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, 0);
    }
    return true;
}

static bool projection_fp_leave(KiNativePilot *pilot, const fenv_t *saved)
{
    return fesetenv(saved) == 0 ||
           pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, 0);
}

static bool projection_host_environment(KiNativePilot *pilot)
{
    fenv_t saved;
    uint32_t smallest = UINT32_C(0x00800000);
    uint32_t half = UINT32_C(0x3f000000);
    uint32_t raw;
    float a, b, result;
    memcpy(&a, &smallest, sizeof(a));
    memcpy(&b, &half, sizeof(b));
    if (!projection_fp_enter(pilot, &saved)) return false;
    volatile float va = a, vb = b, vr = va * vb;
    result = vr;
    memcpy(&raw, &result, sizeof(raw));
    if (!projection_fp_leave(pilot, &saved)) return false;
    /* This detects flush-to-zero modes that would silently widen admission. */
    return raw == UINT32_C(0x00400000) ||
           pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
}

static bool projection_float_binary(KiNativePilot *pilot, unsigned fd,
                                    unsigned fs, unsigned ft, unsigned operation)
{
    uint32_t left = (uint32_t)pilot->cpu.fpr[fs];
    uint32_t right = (uint32_t)pilot->cpu.fpr[ft];
    float a, b, result;
    if (!projection_float_value(left) || !projection_float_value(right))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, left);
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    fenv_t saved;
    if (!projection_fp_enter(pilot, &saved)) return false;
    volatile float va = a, vb = b, vr;
    if (operation == 0) vr = va + vb;
    else if (operation == 1) vr = va - vb;
    else if (operation == 2) vr = va * vb;
    else vr = va / vb;
    result = vr;
    uint32_t raw;
    memcpy(&raw, &result, sizeof(raw));
    if (!projection_fp_leave(pilot, &saved)) return false;
    if (!projection_float_value(raw))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = raw;
    return true;
}

static bool projection_int_to_float(KiNativePilot *pilot, unsigned fd, unsigned fs)
{
    fenv_t saved;
    float result;
    if (!projection_fp_enter(pilot, &saved)) return false;
    volatile int32_t input = (int32_t)(uint32_t)pilot->cpu.fpr[fs];
    volatile float rounded = (float)input;
    result = rounded;
    uint32_t raw;
    memcpy(&raw, &result, sizeof(raw));
    if (!projection_fp_leave(pilot, &saved)) return false;
    if (!projection_float_value(raw))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = raw;
    return true;
}

static bool projection_float_to_int(KiNativePilot *pilot, unsigned fd, unsigned fs)
{
    const uint32_t raw = (uint32_t)pilot->cpu.fpr[fs];
    if (!projection_float_value(raw))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    const unsigned exponent = (raw >> 23) & 0xffu;
    const uint64_t mantissa = exponent ?
        UINT64_C(0x800000) | (raw & UINT32_C(0x7fffff)) : 0;
    const int shift = (int)exponent - 150;
    uint64_t magnitude = 0;
    if (exponent && shift >= 0) {
        if (shift > 7)
            return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
        magnitude = mantissa << shift;
    } else if (exponent) {
        const unsigned cut = (unsigned)-shift;
        uint64_t remainder = mantissa;
        uint64_t halfway = UINT64_MAX;
        if (cut < 64) {
            magnitude = mantissa >> cut;
            remainder = mantissa & ((UINT64_C(1) << cut) - 1);
            halfway = UINT64_C(1) << (cut - 1);
        }
        /* Round halfway to an even integer, matching the admitted FCSR mode. */
        if (remainder > halfway || (remainder == halfway && (magnitude & 1)))
            ++magnitude;
    }
    const int64_t value = (raw >> 31) ? -(int64_t)magnitude : (int64_t)magnitude;
    if (value < INT32_MIN || value > INT32_MAX)
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    pilot->cpu.fpr[fd] = (uint32_t)(int32_t)value;
    return true;
}

static bool projection_compare(KiNativePilot *pilot, unsigned fs, unsigned ft)
{
    uint32_t left = (uint32_t)pilot->cpu.fpr[fs];
    uint32_t right = (uint32_t)pilot->cpu.fpr[ft];
    float a, b;
    if (!projection_float_value(left) || !projection_float_value(right))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, left);
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    pilot->cpu.fcc = a < b;
    pilot->cpu.fcc_valid = true;
    return true;
}

static bool projection_float_unary(KiNativePilot *pilot, unsigned fd,
                                   unsigned fs, uint32_t sign_operation)
{
    uint32_t raw = (uint32_t)pilot->cpu.fpr[fs];
    if (!projection_float_value(raw))
        return pilot_stop(pilot, KI_PILOT_STOP_FP_DOMAIN, pilot->cpu.pc, raw);
    if (sign_operation == 1) raw &= UINT32_C(0x7fffffff);
    else if (sign_operation == 2) raw ^= UINT32_C(0x80000000);
    pilot->cpu.fpr[fd] = raw;
    return true;
}

static bool write_u16(KiNativePilot *pilot, uint64_t address, uint16_t value)
{
    return aligned(pilot, address, 2) &&
           (ki_memory_write_u16_le(&pilot->memory, address, value) == KI_MEMORY_OK ||
            pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address));
}

static bool write_u32(KiNativePilot *pilot, uint64_t address, uint32_t value)
{
    return aligned(pilot, address, 4) &&
           (ki_memory_write_u32_le(&pilot->memory, address, value) == KI_MEMORY_OK ||
            pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, address));
}

static bool write_u64(KiNativePilot *pilot, uint64_t address, uint64_t value)
{
    return aligned(pilot,address,8) && write_u32(pilot,address,(uint32_t)value) &&
           write_u32(pilot,address+4u,(uint32_t)(value>>32));
}

static bool dispatch_targets_enabled(const KiNativePilot *pilot)
{
    return pilot->transaction_kind == KI_PILOT_TRANSACTION_DISPATCH ||
           pilot->transaction_kind == KI_PILOT_TRANSACTION_CONNECTED;
}

static bool projection_fp_semantics(const KiNativePilot *pilot)
{
    const uint32_t pc = (uint32_t)pilot->cpu.pc;
    if (pilot->transaction_kind == KI_PILOT_TRANSACTION_PROJECTION)
        return true;
    if (pilot->transaction_kind != KI_PILOT_TRANSACTION_CONNECTED)
        return false;
    return (pc >= UINT32_C(0x88001038) && pc < UINT32_C(0x88001238)) ||
           (pc >= UINT32_C(0x8800d390) && pc < UINT32_C(0x8800d4cc)) ||
           (pc >= UINT32_C(0x88019d50) && pc < UINT32_C(0x8801a1e8));
}

static bool schedule_jump(KiNativePilot *pilot, uint64_t target)
{
    const uint32_t pc = (uint32_t)pilot->cpu.pc;
    const uint32_t destination = (uint32_t)target;
    bool accepted = true;
    if (pc == UINT32_C(0x88002080)) {
        uint8_t type;
        if (!read_u8(pilot, pilot->cpu.gpr[30], &type)) return false;
        accepted = (type == 1 || type == 6) ? destination == UINT32_C(0x8800271c) :
                   type == 0x12 ? destination == UINT32_C(0x8800464c) :
                   type == 0x15 ? destination == UINT32_C(0x88004574) :
                   type == 0x1a ? destination == UINT32_C(0x88004e54) : false;
    }
    else if (pc == UINT32_C(0x8800679c))
        accepted = destination == UINT32_C(0x88006c80) ||
                   destination == UINT32_C(0x88006d88) ||
                   (dispatch_targets_enabled(pilot) &&
                    destination == UINT32_C(0x88006944));
    else if (pc == UINT32_C(0x88004204))
        accepted = destination == UINT32_C(0x88004e5c) ||
                   destination == UINT32_C(0x88003884) ||
                   (dispatch_targets_enabled(pilot) &&
                    destination == UINT32_C(0x88004908));
    else if (pc == UINT32_C(0x8800846c))
        accepted = destination == UINT32_C(0x88004e64) ||
                   destination == UINT32_C(0x8800329c) ||
                   (dispatch_targets_enabled(pilot) &&
                    destination == UINT32_C(0x88004a6c));
    else if (pc == UINT32_C(0x880067f4))
        accepted = destination == UINT32_C(0x88004e6c) ||
                   (dispatch_targets_enabled(pilot) &&
                    (destination == UINT32_C(0x8800457c) ||
                     destination == UINT32_C(0x88003854) ||
                     destination == UINT32_C(0x880048f4)));
    else if (pc == UINT32_C(0x88006384))
        accepted = destination == UINT32_C(0x88006cb4) ||
                   (dispatch_targets_enabled(pilot) &&
                    destination == UINT32_C(0x880048ec));
    else if (pc == UINT32_C(0x88005428))
        accepted = destination == UINT32_C(0x8800b20c) ||
                   ((pilot->transaction_kind==KI_PILOT_TRANSACTION_CONTACT ||
                     pilot->transaction_kind==KI_PILOT_TRANSACTION_CONNECTED) &&
                    destination==UINT32_C(0x8800a554));
    else if (pc == UINT32_C(0x880063e4)) accepted = destination == UINT32_C(0x8800b1ec);
    else if (pc == UINT32_C(0x8800b1f4)) accepted = destination == UINT32_C(0x88003d6c);
    else if (pc == UINT32_C(0x8800590c) || pc == UINT32_C(0x88005bd4))
        accepted = destination == UINT32_C(0x880021c0) ||
                   destination == UINT32_C(0x880021c8);
    else if (pc == UINT32_C(0x88008774))
        accepted = destination == UINT32_C(0x880032e4);
    else if (pc == UINT32_C(0x88008a0c))
        accepted = destination == UINT32_C(0x880021ac);
    else if (pc == UINT32_C(0x88008ba8))
        accepted = destination == UINT32_C(0x8800892c) ||
                   destination == UINT32_C(0x8800895c);
    else if (pc == UINT32_C(0x88000994))
        accepted = dispatch_targets_enabled(pilot) &&
                   destination == UINT32_C(0x880009ac);
    else if (pc == UINT32_C(0x8800241c))
        accepted = dispatch_targets_enabled(pilot) &&
                   destination == UINT32_C(0x880009a4);
    else if (pc == UINT32_C(0x880085c4))
        accepted = dispatch_targets_enabled(pilot) &&
           (destination == UINT32_C(0x88005534) ||
                    destination == UINT32_C(0x880032dc) ||
                    destination == UINT32_C(0x88004a74));
    else if (pc == UINT32_C(0x8800a964))
        accepted = dispatch_targets_enabled(pilot) &&
                   destination == UINT32_C(0x88004c90);
    else if (pilot->transaction_kind == KI_PILOT_TRANSACTION_CONNECTED &&
             (pc == UINT32_C(0x88000d30) || pc == UINT32_C(0x88000d58) ||
              pc == UINT32_C(0x8800d05c) || pc == UINT32_C(0x88008fd0))) {
        if (pc == UINT32_C(0x8800d05c))
            accepted = destination == UINT32_C(0x880012b0);
        else if (pc == UINT32_C(0x88008fd0))
            accepted = destination == UINT32_C(0x880012b8);
        else
            accepted = false;
    }
    else if (pc == UINT32_C(0x880017dc))
        accepted = pilot->transaction_kind == KI_PILOT_TRANSACTION_FRAME &&
                   destination == UINT32_C(0x88001898);
    else if (pilot->transaction_kind==KI_PILOT_TRANSACTION_RENDER) {
        if (pc==UINT32_C(0x88002030)) accepted=destination==UINT32_C(0x88001bcc);
        else if (pc==UINT32_C(0x88010c0c)) accepted=destination==UINT32_C(0x88010cbc);
        else if (pc==UINT32_C(0x88011874) || pc==UINT32_C(0x88011970))
            accepted=destination==UINT32_C(0x8801187c);
        else if (pc==UINT32_C(0x880118b8)) accepted=destination==UINT32_C(0x88011918);
        else if (pc==UINT32_C(0x88010d14)) accepted=destination==UINT32_C(0x88001e68);
        else if (pc==UINT32_C(0x88001e70)) accepted=destination==UINT32_C(0x88001ae4);
    }
    else if (pilot->transaction_kind==KI_PILOT_TRANSACTION_PROJECTION ||
             pilot->transaction_kind==KI_PILOT_TRANSACTION_CONNECTED) {
        if (pc == UINT32_C(0x8800d4c4))
            accepted = destination == UINT32_C(0x880010f8);
        else if (pc == UINT32_C(0x88019e14))
            accepted = destination == UINT32_C(0x88019f80) ||
                       destination == UINT32_C(0x88019fac);
        else if (pc == UINT32_C(0x88019ee4))
            accepted = destination == UINT32_C(0x88019f90) ||
                       destination == UINT32_C(0x88019fbc);
        else if (pc == UINT32_C(0x8801a1e0))
            accepted = destination == UINT32_C(0x88001124) ||
                       destination == UINT32_C(0x88001138);
    }
    if (!accepted)
        return pilot_stop(pilot, KI_PILOT_STOP_UNKNOWN_TARGET, pilot->cpu.pc, target);
    pilot->cpu.delay_pending = true;
    pilot->cpu.delay_target = (uint32_t)target;
    return true;
}

static bool schedule_branch(KiNativePilot *pilot, bool taken, uint64_t target)
{
    const uint32_t pc = (uint32_t)pilot->cpu.pc;
    if (taken && pc == UINT32_C(0x8800667c) &&
        (uint32_t)target == UINT32_C(0x880066f8))
        return pilot_stop(pilot, KI_PILOT_STOP_NULL_SCRIPT, pilot->cpu.pc, target);
    return schedule_jump(pilot, taken ? target : pc + 8u);
}

static bool divide_word(KiNativePilot *pilot, uint64_t left, uint64_t right)
{
    const int32_t dividend = (int32_t)(uint32_t)left;
    const int32_t divisor = (int32_t)(uint32_t)right;
    if (divisor == 0) {
        /* This is compatibility with one observed MAME 0.289 DRC site,
         * not a claim about general R4600 zero-divisor behavior. */
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_CONNECTED &&
            (uint32_t)pilot->cpu.pc == UINT32_C(0x8800914c) &&
            dividend == -0xc00 && pilot->cpu.hi == UINT64_MAX - 2u &&
            pilot->cpu.lo == 8u) {
            pilot->cpu.lo = 0;
            return true;
        }
        return pilot_stop(pilot, KI_PILOT_STOP_DIVISION,
                          pilot->cpu.pc, right);
    }
    if (dividend == INT32_MIN && divisor == -1)
        return pilot_stop(pilot, KI_PILOT_STOP_DIVISION,
                          pilot->cpu.pc, right);
    pilot->cpu.lo = sign32((uint32_t)(dividend / divisor));
    pilot->cpu.hi = sign32((uint32_t)(dividend % divisor));
    return true;
}

#define G(n) (pilot->cpu.gpr[(n)])
#define SIGNED_LT_ZERO(v) (((v) >> 63) != 0)
#define SIGNED_LE_ZERO(v) ((v) == 0 || SIGNED_LT_ZERO(v))
#define SIGNED_GT_ZERO(v) ((v) != 0 && !SIGNED_LT_ZERO(v))
#define NOP() ((void)0)
#define SLL(d,t,s) set_gpr(pilot,d,sign32((uint32_t)G(t) << (s)))
#define SRL(d,t,s) set_gpr(pilot,d,ki_mips_srl_word(G(t),(s)))
#define SRA(d,t,s) set_gpr(pilot,d,sign32(arithmetic_shift_right32((uint32_t)G(t),(s))))
#define JR(s) do { if (!schedule_jump(pilot,G(s))) return false; } while (0)
#define BREAK(detail) return pilot_stop(pilot,KI_PILOT_STOP_ARITHMETIC,pilot->cpu.pc,(detail))
#define MFHI(d) set_gpr(pilot,d,pilot->cpu.hi)
#define MFLO(d) set_gpr(pilot,d,pilot->cpu.lo)
#define MULT(s,t) do { const int64_t product=(int64_t)(int32_t)(uint32_t)G(s)*(int64_t)(int32_t)(uint32_t)G(t); pilot->cpu.lo=sign32((uint32_t)product); pilot->cpu.hi=sign32((uint32_t)((uint64_t)product>>32)); } while (0)
#define DIV(s,t) do { if (!divide_word(pilot,G(s),G(t))) return false; } while (0)
#define DIVU(s,t) do { const uint32_t dividend=(uint32_t)G(s), divisor=(uint32_t)G(t); if (divisor==0) return pilot_stop(pilot,KI_PILOT_STOP_DIVISION,pilot->cpu.pc,G(t)); pilot->cpu.lo=sign32(dividend/divisor); pilot->cpu.hi=sign32(dividend%divisor); } while (0)
#define MFC0(t,d) do { uint32_t v; if((d)==9){if(!consume_service(pilot,KI_PILOT_SERVICE_COUNT,0,&v))return false;}else if((d)==12)v=pilot->cpu.status_register;else return pilot_stop(pilot,KI_PILOT_STOP_UNKNOWN_TARGET,pilot->cpu.pc,(d));set_gpr(pilot,t,sign32(v)); } while(0)
#define MTC0(t,d) do { if((d)!=12)return pilot_stop(pilot,KI_PILOT_STOP_UNKNOWN_TARGET,pilot->cpu.pc,(d));pilot->cpu.status_register=(uint32_t)G(t); } while(0)
#define MTC1(t,d) (pilot->cpu.fpr[(d)]=(uint32_t)G(t))
#define MFC1(t,s) set_gpr(pilot,t,sign32((uint32_t)pilot->cpu.fpr[(s)]))
#define CVT_S_W(d,s) do { if(projection_fp_semantics(pilot)){if(!projection_int_to_float(pilot,d,s))return false;}else if(!exact_int_to_float(pilot,d,(uint32_t)pilot->cpu.fpr[(s)]))return false; } while(0)
#define SQRT_S(d,s) do { if(!exact_float_sqrt(pilot,d,s))return false; } while(0)
#define CVT_W_S(d,s) do { if(projection_fp_semantics(pilot)){if(!projection_float_to_int(pilot,d,s))return false;}else if(!exact_float_to_int(pilot,d,s))return false; } while(0)
#define LWC1(t,s,i) do { uint32_t v;if(!read_u32(pilot,G(s)+(int64_t)(i),&v))return false;pilot->cpu.fpr[(t)]=v; } while(0)
#define SWC1(t,s,i) do { if(!write_u32(pilot,G(s)+(int64_t)(i),(uint32_t)pilot->cpu.fpr[(t)]))return false; } while(0)
#define ADD_S(d,s,t) do { if(!projection_float_binary(pilot,d,s,t,0))return false; } while(0)
#define SUB_S(d,s,t) do { if(!projection_float_binary(pilot,d,s,t,1))return false; } while(0)
#define MUL_S(d,s,t) do { if(!projection_float_binary(pilot,d,s,t,2))return false; } while(0)
#define DIV_S(d,s,t) do { if(!projection_float_binary(pilot,d,s,t,3))return false; } while(0)
#define ABS_S(d,s) do { if(!projection_float_unary(pilot,d,s,1))return false; } while(0)
#define MOV_S(d,s) do { if(!projection_float_unary(pilot,d,s,0))return false; } while(0)
#define NEG_S(d,s) do { if(!projection_float_unary(pilot,d,s,2))return false; } while(0)
#define C_OLT_S(s,t) do { if(!projection_compare(pilot,s,t))return false; } while(0)
#define ADDU(d,s,t) set_gpr(pilot,d,sign32((uint32_t)G(s)+(uint32_t)G(t)))
#define SUBU(d,s,t) set_gpr(pilot,d,sign32((uint32_t)G(s)-(uint32_t)G(t)))
#define SUB(d,s,t) do { int32_t r;if(!ki_native_pilot_sub_word((int32_t)(uint32_t)G(s),(int32_t)(uint32_t)G(t),&r))return pilot_stop(pilot,KI_PILOT_STOP_ARITHMETIC,pilot->cpu.pc,G(s));set_gpr(pilot,d,sign32((uint32_t)r)); } while(0)
#define AND(d,s,t) set_gpr(pilot,d,ki_mips_and(G(s),G(t)))
#define OR(d,s,t) set_gpr(pilot,d,G(s)|G(t))
#define XOR(d,s,t) set_gpr(pilot,d,G(s)^G(t))
#define SLT(d,s,t) set_gpr(pilot,d,signed64_less(G(s),G(t)))
#define JUMP(a) do { if (!schedule_jump(pilot,(a))) return false; } while (0)
#define JAL(a) do { set_gpr(pilot,31,sign32((uint32_t)pilot->cpu.pc+8u)); JUMP(a); } while (0)
#define BRANCH(c,a) do { if (!schedule_branch(pilot,(c),sign32(a))) return false; } while (0)
#define FP_BRANCH(expect,a) do { if(!pilot->cpu.fcc_valid)return pilot_stop(pilot,KI_PILOT_STOP_FP_DOMAIN,pilot->cpu.pc,0);BRANCH(pilot->cpu.fcc==(expect),a); } while(0)
#define ADDIU(t,s,i) set_gpr(pilot,t,sign32((uint32_t)G(s)+(uint32_t)(int32_t)(i)))
#define SLTI(t,s,i) set_gpr(pilot,t,signed64_less(G(s),(uint64_t)(int64_t)(i)))
#define ANDI(t,s,i) set_gpr(pilot,t,ki_mips_andi(G(s),(uint16_t)(i)))
#define ORI(t,s,i) set_gpr(pilot,t,ki_mips_ori(G(s),(uint16_t)(i)))
#define XORI(t,s,i) set_gpr(pilot,t,G(s)^(uint64_t)(i))
#define LUI(t,i) set_gpr(pilot,t,sign32((uint32_t)(i)<<16))
#define LBU(t,s,i) do { uint8_t v; if(!read_u8(pilot,G(s)+(int64_t)(i),&v))return false; set_gpr(pilot,t,v); } while(0)
#define LB(t,s,i) do { uint8_t v; if(!read_u8(pilot,G(s)+(int64_t)(i),&v))return false; set_gpr(pilot,t,sign32((uint32_t)(int32_t)(int8_t)v)); } while(0)
#define LHU(t,s,i) do { uint16_t v; if(!read_u16(pilot,G(s)+(int64_t)(i),&v))return false; set_gpr(pilot,t,v); } while(0)
#define LH(t,s,i) do { uint16_t v; if(!read_u16(pilot,G(s)+(int64_t)(i),&v))return false; set_gpr(pilot,t,sign32((uint32_t)(int32_t)(int16_t)v)); } while(0)
#define LW(t,s,i) do { uint32_t v; if(!read_u32(pilot,G(s)+(int64_t)(i),&v))return false; set_gpr(pilot,t,sign32(v)); } while(0)
#define SB(t,s,i) do { if(!write_u8(pilot,G(s)+(int64_t)(i),(uint8_t)G(t)))return false; } while(0)
#define SH(t,s,i) do { if(!write_u16(pilot,G(s)+(int64_t)(i),(uint16_t)G(t)))return false; } while(0)
#define SW(t,s,i) do { if(!write_u32(pilot,G(s)+(int64_t)(i),(uint32_t)G(t)))return false; } while(0)
#define LD(t,s,i) do { uint64_t v;if(!read_u64(pilot,G(s)+(int64_t)(i),&v))return false;set_gpr(pilot,t,v); } while(0)
#define SD(t,s,i) do { if(!write_u64(pilot,G(s)+(int64_t)(i),G(t)))return false; } while(0)

#include "generated/ki15d_native_pilot.inc"

static bool pc_is_region_source(uint32_t pc);

static bool source_identity(KiNativePilot *pilot)
{
    for (size_t index = 0; index < sizeof(generated_words)/sizeof(generated_words[0]); ++index) {
        /* Static-region words have their own admission identity and must not
         * silently broaden the legacy effect transactions' source contract. */
        if (pc_is_region_source(generated_words[index].pc)) continue;
        uint32_t actual;
        if (!read_u32(pilot, generated_words[index].pc, &actual)) return false;
        if (actual != generated_words[index].word)
            return pilot_stop(pilot, KI_PILOT_STOP_CODE_IDENTITY,
                              generated_words[index].pc, actual);
    }
    for (size_t index = 0; index < sizeof(source_data_bytes)/sizeof(source_data_bytes[0]); ++index) {
        uint8_t actual;
        if (!read_u8(pilot, source_data_bytes[index].address, &actual)) return false;
        if (actual != source_data_bytes[index].value)
            return pilot_stop(pilot, KI_PILOT_STOP_CODE_IDENTITY,
                              source_data_bytes[index].address, actual);
    }
    for (size_t index = 0; index < sizeof(manual_source_words)/sizeof(manual_source_words[0]); ++index) {
        uint32_t actual;
        if (!read_u32(pilot, manual_source_words[index].pc, &actual)) return false;
        if (actual != manual_source_words[index].word)
            return pilot_stop(pilot, KI_PILOT_STOP_CODE_IDENTITY,
                              manual_source_words[index].pc, actual);
    }
    return true;
}

enum { KI_PILOT_TRANSACTION_REGION_BASE = 7 };

typedef struct KiPilotRegionRegistration {
    KiPilotRegionDescriptor descriptor;
    uint32_t transaction_tag;
} KiPilotRegionRegistration;

static const KiPilotRegionRegistration region_registrations[] = {
    {{KI_PILOT_REGION_STARTUP_CLEAR, UINT32_C(0x880001c4),
      UINT32_C(0x880001ec), 10,
      "855f53840283a2144b0dd564890c228a3893dac0c1986ef022c97a78fcaf601d",
      "complete CPU context and 1 MiB main RAM; no low RAM or service effects"},
     KI_PILOT_TRANSACTION_REGION_BASE}
};

static const KiPilotRegionRegistration *region_for_id(KiPilotRegionId id)
{
    for (size_t i = 0; i < sizeof(region_registrations) / sizeof(region_registrations[0]); ++i)
        if (region_registrations[i].descriptor.id == id) return &region_registrations[i];
    return NULL;
}

static const KiPilotRegionRegistration *region_for_transaction(uint32_t tag)
{
    for (size_t i = 0; i < sizeof(region_registrations) / sizeof(region_registrations[0]); ++i)
        if (region_registrations[i].transaction_tag == tag) return &region_registrations[i];
    return NULL;
}

static bool pc_is_region_source(uint32_t pc)
{
    for (size_t i = 0; i < sizeof(region_registrations) / sizeof(region_registrations[0]); ++i) {
        const KiPilotRegionDescriptor *d = &region_registrations[i].descriptor;
        if (pc >= d->entry_pc && pc < d->entry_pc + d->source_word_count * 4u)
            return true;
    }
    return false;
}

static bool region_source_identity(KiNativePilot *pilot,
                                   const KiPilotRegionRegistration *region)
{
    const uint32_t begin = region->descriptor.entry_pc;
    const uint32_t end = begin + region->descriptor.source_word_count * 4u;
    size_t found = 0;
    for (size_t i = 0; i < sizeof(generated_words) / sizeof(generated_words[0]); ++i) {
        if (generated_words[i].pc < begin || generated_words[i].pc >= end) continue;
        uint32_t actual;
        ++found;
        if (!read_u32(pilot, generated_words[i].pc, &actual)) return false;
        if (actual != generated_words[i].word)
            return pilot_stop(pilot, KI_PILOT_STOP_CODE_IDENTITY,
                              generated_words[i].pc, actual);
    }
    return found == region->descriptor.source_word_count ||
           pilot_stop(pilot, KI_PILOT_STOP_CODE_IDENTITY, begin, (uint32_t)found);
}

static bool frame_data_preconditions(KiNativePilot *pilot)
{
    uint32_t root_word, row, region_begin, region_end;
    uint8_t wanted, token;
    if (!read_u32(pilot, UINT32_C(0x88090140), &root_word) ||
        !read_u8(pilot, G(30) + 0x34u, &wanted) ||
        !read_u8(pilot, G(30) + 0x14u, &token)) return false;
    row = root_word;
    if (row >= UINT32_C(0x88094e00) && row < UINT32_C(0x88099718)) {
        region_begin = UINT32_C(0x88094e00); region_end = UINT32_C(0x88099718);
    } else if (row >= UINT32_C(0x880f0000) && row < UINT32_C(0x880f00c0)) {
        region_begin = UINT32_C(0x880f0000); region_end = UINT32_C(0x880f00c0);
    } else {
        return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, row);
    }
    for (unsigned rows = 0; rows < 64; ++rows) {
        uint8_t key, count;
        uint32_t length_word, selected, next;
        if ((row & 3u) != 0 || row < region_begin || row + 8u > region_end)
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, row);
        if (!read_u8(pilot,row,&key) || !read_u32(pilot,row+4u,&length_word))
            return false;
        const int32_t length = (int32_t)length_word;
        if (length <= 0 || (length & 3) != 0 || (uint32_t)length > region_end-row)
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, length_word);
        if (key != wanted) { row += (uint32_t)length; continue; }
        if (!read_u8(pilot,row+2u,&count)) return false;
        if (count == 0)
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, count);
        uint32_t index = token == 0 ? 1u : token;
        if (index > count) index = count;
        if (row + 4u + index*4u + 4u > row + (uint32_t)length ||
            !read_u32(pilot,row+4u+index*4u,&selected))
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, index);
        if (index == count) next = (uint32_t)length;
        else if (!read_u32(pilot,row+8u+index*4u,&next)) return false;
        while (selected == (uint32_t)length && index != 1u) {
            --index;
            if (!read_u32(pilot,row+4u+index*4u,&selected)) return false;
        }
        if (selected >= (uint32_t)length || next > (uint32_t)length || next < 5u)
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, selected);
        return true;
    }
    return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, row);
}

void ki_native_pilot_bind(KiNativePilot *pilot, uint8_t *ram, size_t ram_size)
{
    memset(pilot, 0, sizeof(*pilot));
    pilot->memory.main_ram = ram;
    pilot->memory.main_ram_size = ram_size;
    pilot->status = KI_PILOT_READY;
    pilot->transaction_kind = KI_PILOT_TRANSACTION_RECORD;
}

bool ki_native_pilot_bind_low_ram(KiNativePilot *pilot, uint8_t *low_ram,
                                  size_t low_ram_size)
{
    if (!pilot || !low_ram || low_ram_size!=KI_PILOT_LOW_RAM_SIZE) return false;
    pilot->memory.low_ram=low_ram;pilot->memory.low_ram_size=low_ram_size;return true;
}

bool ki_native_pilot_begin(KiNativePilot *pilot, uint64_t fp, uint64_t gp)
{
    if (pilot == NULL || pilot->status < KI_PILOT_COMPLETE_ACTIVE ||
        pilot->status > KI_PILOT_COMPLETE_MANUAL || pilot->cpu.delay_pending)
        return false;
    pilot->cpu.gpr[30] = fp;
    pilot->cpu.gpr[28] = gp;
    pilot->cpu.gpr[0] = 0;
    pilot->cpu.pc = UINT32_C(0x88002060);
    pilot->cpu.delay_target = 0;
    pilot->status = KI_PILOT_READY;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->transaction_kind = KI_PILOT_TRANSACTION_RECORD;
    return true;
}

bool ki_native_pilot_begin_dispatch(KiNativePilot *pilot,
                                    uint64_t return_address)
{
    if (pilot == NULL || (pilot->status != KI_PILOT_READY &&
        (pilot->status < KI_PILOT_COMPLETE_ACTIVE ||
         pilot->status > KI_PILOT_COMPLETE_DISPATCH)) || pilot->cpu.delay_pending)
        return false;
    pilot->cpu.gpr[0] = 0;
    pilot->cpu.gpr[31] = return_address;
    pilot->cpu.pc = UINT32_C(0x88002038);
    pilot->cpu.delay_target = 0;
    pilot->status = KI_PILOT_READY;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->transaction_kind = KI_PILOT_TRANSACTION_DISPATCH;
    return true;
}

bool ki_native_pilot_begin_frame(KiNativePilot *pilot)
{
    if (pilot == NULL || (pilot->status != KI_PILOT_READY &&
        (pilot->status < KI_PILOT_COMPLETE_ACTIVE ||
         pilot->status > KI_PILOT_COMPLETE_DISPATCH) &&
         pilot->status != KI_PILOT_COMPLETE_FRAME) || pilot->cpu.delay_pending)
        return false;
    pilot->cpu.gpr[0] = 0;
    pilot->cpu.pc = UINT32_C(0x880016c4);
    pilot->cpu.delay_target = 0;
    pilot->status = KI_PILOT_READY;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->transaction_kind = KI_PILOT_TRANSACTION_FRAME;
    return true;
}

bool ki_native_pilot_configure_services(KiNativePilot *pilot,
                                        const KiPilotServiceInput *inputs,
                                        size_t input_count,
                                        size_t output_capacity)
{
    if (!pilot || (!inputs && input_count) || input_count>KI_PILOT_SERVICE_INPUT_MAX ||
        output_capacity>KI_PILOT_SERVICE_OUTPUT_MAX || pilot->status!=KI_PILOT_READY)
        return false;
    for (size_t i=0;i<input_count;++i) {
        if ((inputs[i].kind==KI_PILOT_SERVICE_COUNT && inputs[i].address!=0) ||
            (inputs[i].kind==KI_PILOT_SERVICE_MMIO_BYTE &&
             (inputs[i].address!=UINT32_C(0xb0000090) || inputs[i].value>0xffu)) ||
            (inputs[i].kind!=KI_PILOT_SERVICE_COUNT &&
             inputs[i].kind!=KI_PILOT_SERVICE_MMIO_BYTE)) return false;
    }
    memset(pilot->service_inputs,0,sizeof(pilot->service_inputs));
    memcpy(pilot->service_inputs,inputs,input_count*sizeof(*inputs));
    memset(pilot->service_outputs,0,sizeof(pilot->service_outputs));
    pilot->service_input_count=(uint32_t)input_count;pilot->service_input_cursor=0;
    pilot->service_output_count=0;pilot->service_output_capacity=(uint32_t)output_capacity;
    return true;
}

bool ki_native_pilot_begin_contact(KiNativePilot *pilot)
{
    if (!pilot || (pilot->status!=KI_PILOT_READY && pilot->status!=KI_PILOT_COMPLETE_CONTACT) || pilot->cpu.delay_pending ||
        pilot->service_input_count!=7 || pilot->service_output_capacity<12) return false;
    pilot->cpu.gpr[0]=0;pilot->cpu.pc=UINT32_C(0x88008cdc);pilot->cpu.delay_target=0;
    pilot->stop_pc=0;pilot->stop_detail=0;pilot->status=KI_PILOT_READY;
    pilot->service_input_cursor=0;pilot->service_output_count=0;
    memset(pilot->service_outputs,0,sizeof(pilot->service_outputs));
    pilot->transaction_kind=KI_PILOT_TRANSACTION_CONTACT;
    return true;
}

bool ki_native_pilot_begin_render(KiNativePilot *pilot)
{
    if (!pilot || (pilot->status!=KI_PILOT_READY &&
        pilot->status!=KI_PILOT_COMPLETE_RENDER) || pilot->cpu.delay_pending)
        return false;
    pilot->cpu.gpr[0]=0;pilot->cpu.pc=UINT32_C(0x88001b90);
    pilot->cpu.delay_target=0;pilot->status=KI_PILOT_READY;
    pilot->stop_pc=0;pilot->stop_detail=0;
    pilot->transaction_kind=KI_PILOT_TRANSACTION_RENDER;
    return true;
}

bool ki_native_pilot_begin_projection(KiNativePilot *pilot)
{
    if (!pilot || (pilot->status != KI_PILOT_READY &&
        pilot->status != KI_PILOT_COMPLETE_PROJECTION) ||
        pilot->cpu.delay_pending)
        return false;
    pilot->cpu.gpr[0] = 0;
    pilot->cpu.pc = UINT32_C(0x88001038);
    pilot->cpu.delay_target = 0;
    pilot->status = KI_PILOT_READY;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->cpu.fcc = false;
    pilot->cpu.fcc_valid = false;
    pilot->transaction_kind = KI_PILOT_TRANSACTION_PROJECTION;
    return true;
}

bool ki_native_pilot_begin_connected(KiNativePilot *pilot)
{
    if (!pilot || (pilot->status != KI_PILOT_READY &&
        pilot->status != KI_PILOT_COMPLETE_CONNECTED) ||
        pilot->cpu.delay_pending ||
        !((pilot->service_input_count == 0 && pilot->service_output_capacity == 0) ||
          (pilot->service_input_count == 7 && pilot->service_output_capacity >= 12)))
        return false;
    pilot->cpu.gpr[0] = 0;
    pilot->cpu.pc = UINT32_C(0x8800099c);
    pilot->cpu.delay_target = 0;
    pilot->status = KI_PILOT_READY;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->cpu.fcc = false;
    pilot->cpu.fcc_valid = false;
    pilot->service_input_cursor = 0;
    pilot->service_output_count = 0;
    memset(pilot->service_outputs, 0, sizeof(pilot->service_outputs));
    pilot->transaction_kind = KI_PILOT_TRANSACTION_CONNECTED;
    return true;
}

const KiPilotRegionDescriptor *ki_native_pilot_region_descriptor(KiPilotRegionId id)
{
    const KiPilotRegionRegistration *region = region_for_id(id);
    return region ? &region->descriptor : NULL;
}

bool ki_native_pilot_begin_region(KiNativePilot *pilot, KiPilotRegionId id)
{
    const KiPilotRegionRegistration *region = region_for_id(id);
    if (!pilot || !region || pilot->status != KI_PILOT_READY ||
        pilot->cpu.delay_pending || (uint32_t)pilot->cpu.pc != region->descriptor.entry_pc ||
        pilot->service_input_count != 0 || pilot->service_input_cursor != 0 ||
        pilot->service_output_count != 0 || pilot->service_output_capacity != 0)
        return false;
    pilot->stop_pc = 0;
    pilot->stop_detail = 0;
    pilot->transaction_kind = region->transaction_tag;
    return true;
}

static bool preconditions(KiNativePilot *pilot)
{
    const KiPilotRegionRegistration *region =
        region_for_transaction(pilot->transaction_kind);
    const uint32_t fp = (uint32_t)G(30);
    const bool record = pilot->transaction_kind == KI_PILOT_TRANSACTION_RECORD;
    const bool frame = pilot->transaction_kind == KI_PILOT_TRANSACTION_FRAME;
    const bool contact = pilot->transaction_kind == KI_PILOT_TRANSACTION_CONTACT;
    const bool render = pilot->transaction_kind == KI_PILOT_TRANSACTION_RENDER;
    const bool projection=pilot->transaction_kind==KI_PILOT_TRANSACTION_PROJECTION;
    const bool connected=pilot->transaction_kind==KI_PILOT_TRANSACTION_CONNECTED;
    const bool pc_ok = region ?
        (uint32_t)pilot->cpu.pc == region->descriptor.entry_pc : record ?
        ((uint32_t)pilot->cpu.pc == UINT32_C(0x88002060) ||
         (uint32_t)pilot->cpu.pc == UINT32_C(0x880054d0)) :
        frame ? (uint32_t)pilot->cpu.pc == UINT32_C(0x880016c4) :
        contact ? (uint32_t)pilot->cpu.pc == UINT32_C(0x88008cdc) :
        render ? (uint32_t)pilot->cpu.pc == UINT32_C(0x88001b90) :
        projection ? (uint32_t)pilot->cpu.pc==UINT32_C(0x88001038) :
        connected ? (uint32_t)pilot->cpu.pc==UINT32_C(0x8800099c) :
        (pilot->transaction_kind == KI_PILOT_TRANSACTION_DISPATCH &&
         (uint32_t)pilot->cpu.pc == UINT32_C(0x88002038));
    const bool inputs_ok = region ?
        (pilot->service_input_count == 0 && pilot->service_input_cursor == 0 &&
         pilot->service_output_count == 0 && pilot->service_output_capacity == 0) : record ?
        (fp >= UINT32_C(0x8808be00) && fp <= UINT32_C(0x8808dd00) &&
         (fp & 0xffu) == 0 && G(28) <= 31) :
        frame ? ((uint32_t)G(30) == UINT32_C(0x8808c000) && G(5) == 0x1a &&
                 G(3) == 0x11 && (uint32_t)G(31) == UINT32_C(0x88001898)) :
        contact ? ((uint32_t)G(2)==UINT32_C(0x88089a48) &&
                   (uint32_t)G(29)==UINT32_C(0x88087258) &&
                   pilot->cpu.status_register==UINT32_C(0x34008001)) :
        render ? ((uint32_t)G(30)==UINT32_C(0x8808c000) &&
                  (uint32_t)G(31)==UINT32_C(0x88001ae4) &&
                  (uint32_t)G(29)==UINT32_C(0x88087250) &&
                  pilot->memory.low_ram &&
                  pilot->memory.low_ram_size==KI_PILOT_LOW_RAM_SIZE) :
        projection ? ((uint32_t)G(4)==UINT32_C(0x8808c000)&&
                      (uint32_t)G(29)==UINT32_C(0x88087258)&&
                      pilot->cpu.status_register==UINT32_C(0x34008001)) :
        connected ? ((uint32_t)G(29)==UINT32_C(0x88087300) &&
                     pilot->cpu.status_register==UINT32_C(0x34008001)) :
        ((uint32_t)G(29) == UINT32_C(0x88087300));
    if (pilot->memory.main_ram == NULL || pilot->memory.main_ram_size != KI_PILOT_RAM_SIZE ||
        !pc_ok || pilot->cpu.delay_pending || !inputs_ok)
        return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION, pilot->cpu.pc, G(30));
    if (record && (uint32_t)pilot->cpu.pc == UINT32_C(0x88002060)) {
        uint8_t type;
        if (!read_u8(pilot, G(30), &type)) return false;
        if (type != 0 && type != UINT8_C(0x1a))
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION,
                              pilot->cpu.pc, type);
    }
    if (frame) {
        uint8_t type, class_index;
        if (!read_u8(pilot, G(30), &type) ||
            !read_u8(pilot, UINT32_C(0x880341f0) + type, &class_index)) return false;
        if (type != UINT8_C(0x1a) || class_index != UINT8_C(0x11))
            return pilot_stop(pilot, KI_PILOT_STOP_PRECONDITION,
                              pilot->cpu.pc, ((uint32_t)type << 8) | class_index);
        if (!frame_data_preconditions(pilot)) return false;
    }
    if (contact) {
        for (unsigned i=0;i<KI_PILOT_FPR_COUNT;++i)
            if (pilot->cpu.fpr[i]>>32)
                return pilot_stop(pilot,KI_PILOT_STOP_PRECONDITION,
                                  pilot->cpu.pc,(uint32_t)i);
    }
    if(projection || connected) {
        if(!projection_host_environment(pilot))return false;
        for(unsigned i=0;i<KI_PILOT_FPR_COUNT;++i)
            if(pilot->cpu.fpr[i]>>32)
                return pilot_stop(pilot,KI_PILOT_STOP_PRECONDITION,pilot->cpu.pc,i);
    }
    if (render) {
        uint8_t type,display,flags,facing,palette_a,palette_b;
        uint32_t frame;
        if(!read_u8(pilot,G(30),&type)||!read_u8(pilot,G(30)+0x94,&display)||
           !read_u8(pilot,G(30)+0x95,&flags)||!read_u8(pilot,G(30)+0x24,&facing)||
           !read_u8(pilot,G(30)+0x8e,&palette_a)||!read_u8(pilot,G(30)+0x97,&palette_b)||
           !read_u32(pilot,G(30)+0x30,&frame))return false;
        if(type!=0x1a||display!=0x60||flags||facing&0x80||palette_a||palette_b||
           (frame&3u)||frame<UINT32_C(0x88097670)||
           frame+0x101u>UINT32_C(0x88099718))
            return pilot_stop(pilot,KI_PILOT_STOP_PRECONDITION,pilot->cpu.pc,frame);
    }
    return region ? region_source_identity(pilot, region) : source_identity(pilot);
}

KiPilotStatus ki_native_pilot_advance(KiNativePilot *pilot, uint64_t budget)
{
    if (pilot->status != KI_PILOT_READY || !preconditions(pilot)) return pilot->status;
    const uint64_t limit = pilot->instruction_count + budget;
    while (pilot->instruction_count < limit) {
        const uint32_t pc = (uint32_t)pilot->cpu.pc;
        const KiPilotRegionRegistration *region =
            region_for_transaction(pilot->transaction_kind);
        if (region && !pilot->cpu.delay_pending && pc == region->descriptor.exit_pc)
            return pilot_stop(pilot, KI_PILOT_STOP_UNKNOWN_PC, pilot->cpu.pc, 0),
                   pilot->status;
        if (region && (pc < region->descriptor.entry_pc ||
                       pc >= region->descriptor.exit_pc))
            return pilot_stop(pilot, KI_PILOT_STOP_UNKNOWN_PC, pilot->cpu.pc, 0),
                   pilot->status;
        if(pilot->transaction_kind==KI_PILOT_TRANSACTION_PROJECTION&&
           !pilot->cpu.delay_pending&&pc==UINT32_C(0x88001228)&&
           (uint32_t)G(4)==UINT32_C(0x8808c000)&&(uint32_t)G(5)==4u) {
            pilot->status=KI_PILOT_COMPLETE_PROJECTION;
            ++pilot->invocation_sequence;return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_CONNECTED &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x880012b8)) {
            const uint32_t expected_outputs =
                pilot->service_input_count == 0 ? 0u : 12u;
            if (pilot->service_input_cursor != pilot->service_input_count ||
                pilot->service_output_count != expected_outputs)
                return pilot_stop(pilot, KI_PILOT_STOP_SERVICE_INPUT,
                                  pilot->cpu.pc, pilot->service_input_cursor),
                       pilot->status;
            pilot->status = KI_PILOT_COMPLETE_CONNECTED;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pilot->transaction_kind==KI_PILOT_TRANSACTION_RENDER &&
            !pilot->cpu.delay_pending && pc==UINT32_C(0x88001ae4)) {
            pilot->status=KI_PILOT_COMPLETE_RENDER;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_CONTACT &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x88008fbc)) {
            if (pilot->service_input_cursor!=pilot->service_input_count ||
                pilot->service_output_count!=12)
                return pilot_stop(pilot,KI_PILOT_STOP_SERVICE_INPUT,pilot->cpu.pc,
                                  pilot->service_input_cursor),pilot->status;
            pilot->status=KI_PILOT_COMPLETE_CONTACT;++pilot->invocation_sequence;return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_FRAME &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x88001898)) {
            pilot->status = KI_PILOT_COMPLETE_FRAME;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_DISPATCH &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x880009ac)) {
            pilot->status = KI_PILOT_COMPLETE_DISPATCH;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_RECORD &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x88002088)) {
            pilot->status = G(4) == 0 ? KI_PILOT_COMPLETE_RELEASED : KI_PILOT_COMPLETE_ACTIVE;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pilot->transaction_kind == KI_PILOT_TRANSACTION_RECORD &&
            !pilot->cpu.delay_pending && pc == UINT32_C(0x880021c8)) {
            pilot->status = KI_PILOT_COMPLETE_EMPTY;
            ++pilot->invocation_sequence;
            return pilot->status;
        }
        if (pc == UINT32_C(0x880054d0)) {
            const bool direct_manual = pilot->instruction_count == 0;
            const uint64_t fp = G(30), next = sign32((uint32_t)fp + 0x100u);
            uint64_t ignored_next;
            if (ki15d_880054d0(&pilot->memory, fp, &ignored_next) != KI_MEMORY_OK)
                return pilot_stop(pilot, KI_PILOT_STOP_MEMORY, pilot->cpu.pc, fp), pilot->status;
            set_gpr(pilot, 6, next); set_gpr(pilot, 7, next);
            pilot->cpu.pc = (uint32_t)G(31); ++pilot->instruction_count;
            if (direct_manual) {
                pilot->status = KI_PILOT_COMPLETE_MANUAL;
                ++pilot->invocation_sequence;
                return pilot->status;
            }
            continue;
        }
        if (!generated_execute(pilot)) return pilot->status;
    }
    const KiPilotRegionRegistration *region =
        region_for_transaction(pilot->transaction_kind);
    if (region && !pilot->cpu.delay_pending &&
        (uint32_t)pilot->cpu.pc == region->descriptor.exit_pc)
        pilot_stop(pilot, KI_PILOT_STOP_UNKNOWN_PC, pilot->cpu.pc, 0);
    else
        pilot_stop(pilot, KI_PILOT_STOP_BUDGET, pilot->cpu.pc, budget);
    return pilot->status;
}

const char *ki_native_pilot_status_name(KiPilotStatus status)
{
    static const char *const names[] = {"ready","complete-active","complete-released",
        "complete-empty","complete-manual","complete-dispatch","budget","memory","unaligned","unknown-pc","unknown-target",
        "null-script","division","precondition","code-identity","complete-frame",
        "service-input","service-output","fp-domain","complete-contact",
        "arithmetic","complete-render","complete-projection","complete-connected"};
    return (unsigned)status < sizeof(names)/sizeof(names[0]) ? names[status] : "invalid";
}
