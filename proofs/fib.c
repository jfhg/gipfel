// Lean compiler output
// Module: Fib
// Imports: public import Init public meta import Init
#include <lean/lean.h>
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-label"
#elif defined(__GNUC__) && !defined(__CLANG__)
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-label"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#ifdef __cplusplus
extern "C" {
#endif
uint8_t lean_nat_dec_eq(lean_object*, lean_object*);
lean_object* lean_nat_sub(lean_object*, lean_object*);
lean_object* lean_nat_add(lean_object*, lean_object*);
LEAN_EXPORT lean_object* l_fib(lean_object*);
LEAN_EXPORT lean_object* l_fib___boxed(lean_object*);
LEAN_EXPORT lean_object* l_fibLoop(lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___redArg(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___redArg___boxed(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___boxed(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___redArg(lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___redArg___boxed(lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___boxed(lean_object*, lean_object*, lean_object*, lean_object*, lean_object*);
LEAN_EXPORT lean_object* l_fib(lean_object* v_x_1_){
_start:
{
lean_object* v_zero_2_; uint8_t v_isZero_3_; 
v_zero_2_ = lean_unsigned_to_nat(0u);
v_isZero_3_ = lean_nat_dec_eq(v_x_1_, v_zero_2_);
if (v_isZero_3_ == 1)
{
return v_zero_2_;
}
else
{
lean_object* v_one_4_; lean_object* v_n_5_; uint8_t v_isZero_6_; 
v_one_4_ = lean_unsigned_to_nat(1u);
v_n_5_ = lean_nat_sub(v_x_1_, v_one_4_);
v_isZero_6_ = lean_nat_dec_eq(v_n_5_, v_zero_2_);
if (v_isZero_6_ == 1)
{
lean_dec(v_n_5_);
return v_one_4_;
}
else
{
lean_object* v_n_7_; lean_object* v___x_8_; lean_object* v___x_9_; lean_object* v___x_10_; lean_object* v___x_11_; 
v_n_7_ = lean_nat_sub(v_n_5_, v_one_4_);
lean_dec(v_n_5_);
v___x_8_ = lean_nat_add(v_n_7_, v_one_4_);
v___x_9_ = l_fib(v___x_8_);
lean_dec(v___x_8_);
v___x_10_ = l_fib(v_n_7_);
lean_dec(v_n_7_);
v___x_11_ = lean_nat_add(v___x_9_, v___x_10_);
lean_dec(v___x_10_);
lean_dec(v___x_9_);
return v___x_11_;
}
}
}
}
LEAN_EXPORT lean_object* l_fib___boxed(lean_object* v_x_12_){
_start:
{
lean_object* v_res_13_; 
v_res_13_ = l_fib(v_x_12_);
lean_dec(v_x_12_);
return v_res_13_;
}
}
LEAN_EXPORT lean_object* l_fibLoop(lean_object* v_x_14_, lean_object* v_x_15_, lean_object* v_x_16_){
_start:
{
lean_object* v_zero_17_; uint8_t v_isZero_18_; 
v_zero_17_ = lean_unsigned_to_nat(0u);
v_isZero_18_ = lean_nat_dec_eq(v_x_14_, v_zero_17_);
if (v_isZero_18_ == 1)
{
lean_dec(v_x_16_);
lean_dec(v_x_14_);
return v_x_15_;
}
else
{
lean_object* v_one_19_; lean_object* v_n_20_; lean_object* v___x_21_; 
v_one_19_ = lean_unsigned_to_nat(1u);
v_n_20_ = lean_nat_sub(v_x_14_, v_one_19_);
lean_dec(v_x_14_);
v___x_21_ = lean_nat_add(v_x_15_, v_x_16_);
lean_dec(v_x_16_);
{
lean_object* _tmp_0 = v_n_20_;
lean_object* _tmp_1 = v___x_21_;
lean_object* _tmp_2 = v_x_15_;
v_x_14_ = _tmp_0;
v_x_15_ = _tmp_1;
v_x_16_ = _tmp_2;
}
goto _start;
}
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___redArg(lean_object* v_x_23_, lean_object* v_x_24_, lean_object* v_x_25_, lean_object* v_h__1_26_, lean_object* v_h__2_27_){
_start:
{
lean_object* v_zero_28_; uint8_t v_isZero_29_; 
v_zero_28_ = lean_unsigned_to_nat(0u);
v_isZero_29_ = lean_nat_dec_eq(v_x_23_, v_zero_28_);
if (v_isZero_29_ == 1)
{
lean_object* v___x_30_; 
lean_dec(v_h__2_27_);
v___x_30_ = lean_apply_2(v_h__1_26_, v_x_24_, v_x_25_);
return v___x_30_;
}
else
{
lean_object* v_one_31_; lean_object* v_n_32_; lean_object* v___x_33_; 
lean_dec(v_h__1_26_);
v_one_31_ = lean_unsigned_to_nat(1u);
v_n_32_ = lean_nat_sub(v_x_23_, v_one_31_);
v___x_33_ = lean_apply_3(v_h__2_27_, v_n_32_, v_x_24_, v_x_25_);
return v___x_33_;
}
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___redArg___boxed(lean_object* v_x_34_, lean_object* v_x_35_, lean_object* v_x_36_, lean_object* v_h__1_37_, lean_object* v_h__2_38_){
_start:
{
lean_object* v_res_39_; 
v_res_39_ = l___private_Fib_0__fibLoop_match__1_splitter___redArg(v_x_34_, v_x_35_, v_x_36_, v_h__1_37_, v_h__2_38_);
lean_dec(v_x_34_);
return v_res_39_;
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter(lean_object* v_motive_40_, lean_object* v_x_41_, lean_object* v_x_42_, lean_object* v_x_43_, lean_object* v_h__1_44_, lean_object* v_h__2_45_){
_start:
{
lean_object* v_zero_46_; uint8_t v_isZero_47_; 
v_zero_46_ = lean_unsigned_to_nat(0u);
v_isZero_47_ = lean_nat_dec_eq(v_x_41_, v_zero_46_);
if (v_isZero_47_ == 1)
{
lean_object* v___x_48_; 
lean_dec(v_h__2_45_);
v___x_48_ = lean_apply_2(v_h__1_44_, v_x_42_, v_x_43_);
return v___x_48_;
}
else
{
lean_object* v_one_49_; lean_object* v_n_50_; lean_object* v___x_51_; 
lean_dec(v_h__1_44_);
v_one_49_ = lean_unsigned_to_nat(1u);
v_n_50_ = lean_nat_sub(v_x_41_, v_one_49_);
v___x_51_ = lean_apply_3(v_h__2_45_, v_n_50_, v_x_42_, v_x_43_);
return v___x_51_;
}
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fibLoop_match__1_splitter___boxed(lean_object* v_motive_52_, lean_object* v_x_53_, lean_object* v_x_54_, lean_object* v_x_55_, lean_object* v_h__1_56_, lean_object* v_h__2_57_){
_start:
{
lean_object* v_res_58_; 
v_res_58_ = l___private_Fib_0__fibLoop_match__1_splitter(v_motive_52_, v_x_53_, v_x_54_, v_x_55_, v_h__1_56_, v_h__2_57_);
lean_dec(v_x_53_);
return v_res_58_;
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___redArg(lean_object* v_x_59_, lean_object* v_h__1_60_, lean_object* v_h__2_61_, lean_object* v_h__3_62_){
_start:
{
lean_object* v_zero_63_; uint8_t v_isZero_64_; 
v_zero_63_ = lean_unsigned_to_nat(0u);
v_isZero_64_ = lean_nat_dec_eq(v_x_59_, v_zero_63_);
if (v_isZero_64_ == 1)
{
lean_object* v___x_65_; lean_object* v___x_66_; 
lean_dec(v_h__3_62_);
lean_dec(v_h__2_61_);
v___x_65_ = lean_box(0);
v___x_66_ = lean_apply_1(v_h__1_60_, v___x_65_);
return v___x_66_;
}
else
{
lean_object* v_one_67_; lean_object* v_n_68_; uint8_t v_isZero_69_; 
lean_dec(v_h__1_60_);
v_one_67_ = lean_unsigned_to_nat(1u);
v_n_68_ = lean_nat_sub(v_x_59_, v_one_67_);
v_isZero_69_ = lean_nat_dec_eq(v_n_68_, v_zero_63_);
if (v_isZero_69_ == 1)
{
lean_object* v___x_70_; lean_object* v___x_71_; 
lean_dec(v_n_68_);
lean_dec(v_h__3_62_);
v___x_70_ = lean_box(0);
v___x_71_ = lean_apply_1(v_h__2_61_, v___x_70_);
return v___x_71_;
}
else
{
lean_object* v_n_72_; lean_object* v___x_73_; 
lean_dec(v_h__2_61_);
v_n_72_ = lean_nat_sub(v_n_68_, v_one_67_);
lean_dec(v_n_68_);
v___x_73_ = lean_apply_1(v_h__3_62_, v_n_72_);
return v___x_73_;
}
}
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___redArg___boxed(lean_object* v_x_74_, lean_object* v_h__1_75_, lean_object* v_h__2_76_, lean_object* v_h__3_77_){
_start:
{
lean_object* v_res_78_; 
v_res_78_ = l___private_Fib_0__fib_match__1_splitter___redArg(v_x_74_, v_h__1_75_, v_h__2_76_, v_h__3_77_);
lean_dec(v_x_74_);
return v_res_78_;
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter(lean_object* v_motive_79_, lean_object* v_x_80_, lean_object* v_h__1_81_, lean_object* v_h__2_82_, lean_object* v_h__3_83_){
_start:
{
lean_object* v_zero_84_; uint8_t v_isZero_85_; 
v_zero_84_ = lean_unsigned_to_nat(0u);
v_isZero_85_ = lean_nat_dec_eq(v_x_80_, v_zero_84_);
if (v_isZero_85_ == 1)
{
lean_object* v___x_86_; lean_object* v___x_87_; 
lean_dec(v_h__3_83_);
lean_dec(v_h__2_82_);
v___x_86_ = lean_box(0);
v___x_87_ = lean_apply_1(v_h__1_81_, v___x_86_);
return v___x_87_;
}
else
{
lean_object* v_one_88_; lean_object* v_n_89_; uint8_t v_isZero_90_; 
lean_dec(v_h__1_81_);
v_one_88_ = lean_unsigned_to_nat(1u);
v_n_89_ = lean_nat_sub(v_x_80_, v_one_88_);
v_isZero_90_ = lean_nat_dec_eq(v_n_89_, v_zero_84_);
if (v_isZero_90_ == 1)
{
lean_object* v___x_91_; lean_object* v___x_92_; 
lean_dec(v_n_89_);
lean_dec(v_h__3_83_);
v___x_91_ = lean_box(0);
v___x_92_ = lean_apply_1(v_h__2_82_, v___x_91_);
return v___x_92_;
}
else
{
lean_object* v_n_93_; lean_object* v___x_94_; 
lean_dec(v_h__2_82_);
v_n_93_ = lean_nat_sub(v_n_89_, v_one_88_);
lean_dec(v_n_89_);
v___x_94_ = lean_apply_1(v_h__3_83_, v_n_93_);
return v___x_94_;
}
}
}
}
LEAN_EXPORT lean_object* l___private_Fib_0__fib_match__1_splitter___boxed(lean_object* v_motive_95_, lean_object* v_x_96_, lean_object* v_h__1_97_, lean_object* v_h__2_98_, lean_object* v_h__3_99_){
_start:
{
lean_object* v_res_100_; 
v_res_100_ = l___private_Fib_0__fib_match__1_splitter(v_motive_95_, v_x_96_, v_h__1_97_, v_h__2_98_, v_h__3_99_);
lean_dec(v_x_96_);
return v_res_100_;
}
}
lean_object* initialize_Init(uint8_t builtin);
lean_object* initialize_Init(uint8_t builtin);
void lean_initialize_runtime_module();
static bool _G_initialized = false;
LEAN_EXPORT lean_object* initialize_Fib(uint8_t builtin) {
lean_object * res;
if (_G_initialized) return lean_io_result_mk_ok(lean_box(0));
_G_initialized = true;
lean_initialize_runtime_module();
res = initialize_Init(builtin);
if (lean_io_result_is_error(res)) return res;
lean_dec_ref(res);
res = initialize_Init(builtin);
if (lean_io_result_is_error(res)) return res;
lean_dec_ref(res);
return lean_io_result_mk_ok(lean_box(0));
}
#ifdef __cplusplus
}
#endif
