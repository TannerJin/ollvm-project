; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --function-signature --scrub-attributes
; RUN: opt -attributor -attributor-manifest-internal  -attributor-max-iterations-verify -attributor-annotate-decl-cs -attributor-max-iterations=11 -S < %s | FileCheck %s --check-prefixes=CHECK,NOT_CGSCC_NPM,NOT_CGSCC_OPM,NOT_TUNIT_NPM,IS__TUNIT____,IS________OPM,IS__TUNIT_OPM
; RUN: opt -aa-pipeline=basic-aa -passes=attributor -attributor-manifest-internal  -attributor-max-iterations-verify -attributor-annotate-decl-cs -attributor-max-iterations=11 -S < %s | FileCheck %s --check-prefixes=CHECK,NOT_CGSCC_OPM,NOT_CGSCC_NPM,NOT_TUNIT_OPM,IS__TUNIT____,IS________NPM,IS__TUNIT_NPM
; RUN: opt -attributor-cgscc -attributor-manifest-internal  -attributor-annotate-decl-cs -S < %s | FileCheck %s --check-prefixes=CHECK,NOT_TUNIT_NPM,NOT_TUNIT_OPM,NOT_CGSCC_NPM,IS__CGSCC____,IS________OPM,IS__CGSCC_OPM
; RUN: opt -aa-pipeline=basic-aa -passes=attributor-cgscc -attributor-manifest-internal  -attributor-annotate-decl-cs -S < %s | FileCheck %s --check-prefixes=CHECK,NOT_TUNIT_NPM,NOT_TUNIT_OPM,NOT_CGSCC_OPM,IS__CGSCC____,IS________NPM,IS__CGSCC_NPM

target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"

; Test cases specifically designed for the "nofree" function attribute.
; We use FIXME's to indicate problems and missing attributes.

; Free functions
declare void @free(i8* nocapture) local_unnamed_addr #1
declare noalias i8* @realloc(i8* nocapture, i64) local_unnamed_addr #0
declare void @_ZdaPv(i8*) local_unnamed_addr #2


; TEST 1 (positive case)
; IS__TUNIT____: Function Attrs: nofree noinline nosync nounwind readnone uwtable
; IS__CGSCC____: Function Attrs: nofree noinline norecurse nosync nounwind readnone uwtable
define void @only_return() #0 {
; CHECK-LABEL: define {{[^@]+}}@only_return()
; CHECK-NEXT:    ret void
;
  ret void
}


; TEST 2 (negative case)
; Only free
; void only_free(char* p) {
;    free(p);
; }

; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define void @only_free(i8* nocapture %0) local_unnamed_addr #0 {
; CHECK-LABEL: define {{[^@]+}}@only_free
; CHECK-SAME: (i8* nocapture [[TMP0:%.*]]) local_unnamed_addr
; CHECK-NEXT:    tail call void @free(i8* nocapture [[TMP0]])
; CHECK-NEXT:    ret void
;
  tail call void @free(i8* %0) #1
  ret void
}


; TEST 3 (negative case)
; Free occurs in same scc.
; void free_in_scc1(char*p){
;    free_in_scc2(p);
; }
; void free_in_scc2(char*p){
;    free_in_scc1(p);
;    free(p);
; }


define void @free_in_scc1(i8* nocapture %0) local_unnamed_addr #0 {
; CHECK-LABEL: define {{[^@]+}}@free_in_scc1
; CHECK-SAME: (i8* nocapture [[TMP0:%.*]]) local_unnamed_addr
; CHECK-NEXT:    tail call void @free_in_scc2(i8* nocapture [[TMP0]])
; CHECK-NEXT:    ret void
;
  tail call void @free_in_scc2(i8* %0) #1
  ret void
}


; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define void @free_in_scc2(i8* nocapture %0) local_unnamed_addr #0 {
; CHECK-LABEL: define {{[^@]+}}@free_in_scc2
; CHECK-SAME: (i8* nocapture [[TMP0:%.*]]) local_unnamed_addr
; CHECK-NEXT:    [[CMP:%.*]] = icmp eq i8* [[TMP0]], null
; CHECK-NEXT:    br i1 [[CMP]], label [[REC:%.*]], label [[CALL:%.*]]
; CHECK:       call:
; CHECK-NEXT:    tail call void @free(i8* nocapture [[TMP0]])
; CHECK-NEXT:    br label [[END:%.*]]
; CHECK:       rec:
; CHECK-NEXT:    tail call void @free_in_scc1(i8* nocapture [[TMP0]])
; CHECK-NEXT:    br label [[END]]
; CHECK:       end:
; CHECK-NEXT:    ret void
;
  %cmp = icmp eq i8* %0, null
  br i1 %cmp, label %rec, label %call
call:
  tail call void @free(i8* %0) #1
  br label %end
rec:
  tail call void @free_in_scc1(i8* %0)
  br label %end
end:
  ret void
}


; TEST 4 (positive case)
; Free doesn't occur.
; void mutual_recursion1(){
;    mutual_recursion2();
; }
; void mutual_recursion2(){
;     mutual_recursion1();
; }


; NOT_CGSCC_NPM: Function Attrs: nofree noinline noreturn nosync nounwind readnone uwtable
; IS__CGSCC_NPM: Function Attrs: nofree noinline norecurse noreturn nosync nounwind readnone uwtable
define void @mutual_recursion1() #0 {
; CHECK-LABEL: define {{[^@]+}}@mutual_recursion1()
; CHECK-NEXT:    unreachable
;
  call void @mutual_recursion2()
  ret void
}

; NOT_CGSCC_NPM: Function Attrs: nofree noinline noreturn nosync nounwind readnone uwtable
; IS__CGSCC_NPM: Function Attrs: nofree noinline norecurse noreturn nosync nounwind readnone uwtable
define void @mutual_recursion2() #0 {
; CHECK-LABEL: define {{[^@]+}}@mutual_recursion2()
; CHECK-NEXT:    unreachable
;
  call void @mutual_recursion1()
  ret void
}


; TEST 5
; C++ delete operation (negative case)
; void delete_op (char p[]){
;     delete [] p;
; }

; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define void @_Z9delete_opPc(i8* %0) local_unnamed_addr #0 {
; CHECK-LABEL: define {{[^@]+}}@_Z9delete_opPc
; CHECK-SAME: (i8* [[TMP0:%.*]]) local_unnamed_addr
; CHECK-NEXT:    [[TMP2:%.*]] = icmp eq i8* [[TMP0]], null
; CHECK-NEXT:    br i1 [[TMP2]], label [[TMP4:%.*]], label [[TMP3:%.*]]
; CHECK:       3:
; CHECK-NEXT:    tail call void @_ZdaPv(i8* nonnull [[TMP0]])
; CHECK-NEXT:    br label [[TMP4]]
; CHECK:       4:
; CHECK-NEXT:    ret void
;
  %2 = icmp eq i8* %0, null
  br i1 %2, label %4, label %3

; <label>:3:                                      ; preds = %1
  tail call void @_ZdaPv(i8* nonnull %0) #2
  br label %4

; <label>:4:                                      ; preds = %3, %1
  ret void
}


; TEST 6 (negative case)
; Call realloc
; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define noalias i8* @call_realloc(i8* nocapture %0, i64 %1) local_unnamed_addr #0 {
; CHECK-LABEL: define {{[^@]+}}@call_realloc
; CHECK-SAME: (i8* nocapture [[TMP0:%.*]], i64 [[TMP1:%.*]]) local_unnamed_addr
; CHECK-NEXT:    [[RET:%.*]] = tail call i8* @realloc(i8* nocapture [[TMP0]], i64 [[TMP1]])
; CHECK-NEXT:    ret i8* [[RET]]
;
  %ret = tail call i8* @realloc(i8* %0, i64 %1) #2
  ret i8* %ret
}


; TEST 7 (positive case)
; Call function declaration with "nofree"


; CHECK: Function Attrs:  nofree noinline nounwind readnone uwtable
; CHECK-NEXT: declare void @nofree_function()
declare void @nofree_function() nofree readnone #0

; IS__TUNIT____: Function Attrs: nofree noinline nosync nounwind readnone uwtable
; IS__CGSCC____: Function Attrs: nofree noinline norecurse nosync nounwind readnone uwtable
define void @call_nofree_function() #0 {
; CHECK-LABEL: define {{[^@]+}}@call_nofree_function()
; CHECK-NEXT:    ret void
;
  tail call void @nofree_function()
  ret void
}

; TEST 8 (negative case)
; Call function declaration without "nofree"


; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NEXT: declare void @maybe_free()
declare void @maybe_free() #0


; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define void @call_maybe_free() #0 {
; CHECK-LABEL: define {{[^@]+}}@call_maybe_free()
; CHECK-NEXT:    tail call void @maybe_free()
; CHECK-NEXT:    ret void
;
  tail call void @maybe_free()
  ret void
}


; TEST 9 (negative case)
; Call both of above functions

; CHECK: Function Attrs: noinline nounwind uwtable
; CHECK-NOT: nofree
define void @call_both() #0 {
; CHECK-LABEL: define {{[^@]+}}@call_both()
; CHECK-NEXT:    tail call void @maybe_free()
; CHECK-NEXT:    ret void
;
  tail call void @maybe_free()
  tail call void @nofree_function()
  ret void
}


; TEST 10 (positive case)
; Call intrinsic function
; CHECK: Function Attrs: nounwind readnone speculatable
; CHECK-NEXT: declare float @llvm.floor.f32(float)
declare float @llvm.floor.f32(float)

; IS__TUNIT____: Function Attrs: nofree noinline nosync nounwind readnone uwtable
; IS__CGSCC____: Function Attrs: nofree noinline norecurse nosync nounwind readnone uwtable
define void @call_floor(float %a) #0 {
; CHECK-LABEL: define {{[^@]+}}@call_floor
; CHECK-SAME: (float [[A:%.*]])
; CHECK-NEXT:    ret void
;
  tail call float @llvm.floor.f32(float %a)
  ret void
}

; FIXME: missing nofree
; CHECK: Function Attrs: noinline nosync nounwind readnone uwtable
define float @call_floor2(float %a) #0 {
; CHECK-LABEL: define {{[^@]+}}@call_floor2
; CHECK-SAME: (float [[A:%.*]])
; CHECK-NEXT:    [[C:%.*]] = tail call float @llvm.floor.f32(float [[A]])
; CHECK-NEXT:    ret float [[C]]
;
  %c = tail call float @llvm.floor.f32(float %a)
  ret float %c
}

; TEST 11 (positive case)
; Check propagation.

; IS__TUNIT____: Function Attrs: nofree noinline nosync nounwind readnone uwtable
; IS__CGSCC____: Function Attrs: nofree noinline norecurse nosync nounwind readnone uwtable
define void @f1() #0 {
; CHECK-LABEL: define {{[^@]+}}@f1()
; CHECK-NEXT:    ret void
;
  tail call void @nofree_function()
  ret void
}

; IS__TUNIT____: Function Attrs: nofree noinline nosync nounwind readnone uwtable
; IS__CGSCC____: Function Attrs: nofree noinline norecurse nosync nounwind readnone uwtable
define void @f2() #0 {
; CHECK-LABEL: define {{[^@]+}}@f2()
; CHECK-NEXT:    ret void
;
  tail call void @f1()
  ret void
}

; TEST 12 NoFree argument - positive.
define double @test12(double* nocapture readonly %a) {
; CHECK-LABEL: define {{[^@]+}}@test12
; CHECK-SAME: (double* nocapture nofree nonnull readonly align 8 dereferenceable(8) [[A:%.*]])
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load double, double* [[A]], align 8
; CHECK-NEXT:    [[CALL:%.*]] = tail call double @cos(double [[TMP0]])
; CHECK-NEXT:    ret double [[CALL]]
;
entry:
  %0 = load double, double* %a, align 8
  %call = tail call double @cos(double %0) #2
  ret double %call
}

declare double @cos(double) nobuiltin nounwind nofree

; FIXME: %a should be nofree.
; TEST 13 NoFree argument - positive.
define noalias i32* @test13(i64* nocapture readonly %a) {
; CHECK-LABEL: define {{[^@]+}}@test13
; CHECK-SAME: (i64* nocapture nonnull readonly align 8 dereferenceable(8) [[A:%.*]])
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP0:%.*]] = load i64, i64* [[A]], align 8
; CHECK-NEXT:    [[CALL:%.*]] = tail call noalias i8* @malloc(i64 [[TMP0]])
; CHECK-NEXT:    [[TMP1:%.*]] = bitcast i8* [[CALL]] to i32*
; CHECK-NEXT:    ret i32* [[TMP1]]
;
entry:
  %0 = load i64, i64* %a, align 8
  %call = tail call noalias i8* @malloc(i64 %0) #2
  %1 = bitcast i8* %call to i32*
  ret i32* %1
}

define void @test14(i8* nocapture %0, i8* nocapture %1) {
; CHECK-LABEL: define {{[^@]+}}@test14
; CHECK-SAME: (i8* nocapture [[TMP0:%.*]], i8* nocapture nofree readnone [[TMP1:%.*]])
; CHECK-NEXT:    tail call void @free(i8* nocapture [[TMP0]])
; CHECK-NEXT:    ret void
;
  tail call void @free(i8* %0) #1
  ret void
}

; UTC_ARGS: --enable

define void @nonnull_assume_pos(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4) {
; ATTRIBUTOR-LABEL: define {{[^@]+}}@nonnull_assume_pos
; ATTRIBUTOR-SAME: (i8* nofree [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* nofree [[ARG3:%.*]], i8* [[ARG4:%.*]])
; ATTRIBUTOR-NEXT:    call void @llvm.assume(i1 true) #11 [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; ATTRIBUTOR-NEXT:    call void @unknown(i8* nofree [[ARG1]], i8* [[ARG2]], i8* nofree [[ARG3]], i8* [[ARG4]])
; ATTRIBUTOR-NEXT:    ret void
;
; CHECK-LABEL: define {{[^@]+}}@nonnull_assume_pos
; CHECK-SAME: (i8* nofree [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* nofree [[ARG3:%.*]], i8* [[ARG4:%.*]])
; CHECK-NEXT:    call void @llvm.assume(i1 true) #11 [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; CHECK-NEXT:    call void @unknown(i8* nofree [[ARG1]], i8* [[ARG2]], i8* nofree [[ARG3]], i8* [[ARG4]])
; CHECK-NEXT:    ret void
;
  call void @llvm.assume(i1 true) ["nofree"(i8* %arg1), "nofree"(i8* %arg3)]
  call void @unknown(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4)
  ret void
}
define void @nonnull_assume_neg(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4) {
; ATTRIBUTOR-LABEL: define {{[^@]+}}@nonnull_assume_neg
; ATTRIBUTOR-SAME: (i8* [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* [[ARG3:%.*]], i8* [[ARG4:%.*]])
; ATTRIBUTOR-NEXT:    call void @unknown(i8* [[ARG1]], i8* [[ARG2]], i8* [[ARG3]], i8* [[ARG4]])
; ATTRIBUTOR-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; ATTRIBUTOR-NEXT:    ret void
;
; CHECK-LABEL: define {{[^@]+}}@nonnull_assume_neg
; CHECK-SAME: (i8* [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* [[ARG3:%.*]], i8* [[ARG4:%.*]])
; CHECK-NEXT:    call void @unknown(i8* [[ARG1]], i8* [[ARG2]], i8* [[ARG3]], i8* [[ARG4]])
; CHECK-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; CHECK-NEXT:    ret void
;
  call void @unknown(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4)
  call void @llvm.assume(i1 true) ["nofree"(i8* %arg1), "nofree"(i8* %arg3)]
  ret void
}
define void @nonnull_assume_call(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4) {
; ATTRIBUTOR-LABEL: define {{[^@]+}}@nonnull_assume_call
; ATTRIBUTOR-SAME: (i8* [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* [[ARG3:%.*]], i8* [[ARG4:%.*]])
; ATTRIBUTOR-NEXT:    call void @unknown(i8* [[ARG1]], i8* [[ARG2]], i8* [[ARG3]], i8* [[ARG4]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr(i8* noalias readnone [[ARG1]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr(i8* noalias readnone [[ARG2]])
; ATTRIBUTOR-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr(i8* noalias nofree readnone [[ARG3]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr(i8* noalias readnone [[ARG4]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr_ret(i8* noalias nofree readnone [[ARG1]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr_ret(i8* noalias readnone [[ARG2]])
; ATTRIBUTOR-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG4]]) ]
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr_ret(i8* noalias nofree readnone [[ARG3]])
; ATTRIBUTOR-NEXT:    call void @use_i8_ptr_ret(i8* noalias nofree readnone [[ARG4]])
; ATTRIBUTOR-NEXT:    ret void
;
; CHECK-LABEL: define {{[^@]+}}@nonnull_assume_call
; CHECK-SAME: (i8* [[ARG1:%.*]], i8* [[ARG2:%.*]], i8* [[ARG3:%.*]], i8* [[ARG4:%.*]])
; CHECK-NEXT:    call void @unknown(i8* [[ARG1]], i8* [[ARG2]], i8* [[ARG3]], i8* [[ARG4]])
; CHECK-NEXT:    call void @use_i8_ptr(i8* noalias nocapture readnone [[ARG1]])
; CHECK-NEXT:    call void @use_i8_ptr(i8* noalias nocapture readnone [[ARG2]])
; CHECK-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG3]]) ]
; CHECK-NEXT:    call void @use_i8_ptr(i8* noalias nocapture nofree readnone [[ARG3]])
; CHECK-NEXT:    call void @use_i8_ptr(i8* noalias nocapture readnone [[ARG4]])
; CHECK-NEXT:    call void @use_i8_ptr_ret(i8* noalias nocapture nofree readnone [[ARG1]])
; CHECK-NEXT:    call void @use_i8_ptr_ret(i8* noalias nocapture readnone [[ARG2]])
; CHECK-NEXT:    call void @llvm.assume(i1 true) [ "nofree"(i8* [[ARG1]]), "nofree"(i8* [[ARG4]]) ]
; CHECK-NEXT:    call void @use_i8_ptr_ret(i8* noalias nocapture nofree readnone [[ARG3]])
; CHECK-NEXT:    call void @use_i8_ptr_ret(i8* noalias nocapture nofree readnone [[ARG4]])
; CHECK-NEXT:    ret void
;
  call void @unknown(i8* %arg1, i8* %arg2, i8* %arg3, i8* %arg4)
  call void @use_i8_ptr(i8* %arg1)
  call void @use_i8_ptr(i8* %arg2)
  call void @llvm.assume(i1 true) ["nofree"(i8* %arg1), "nofree"(i8* %arg3)]
  call void @use_i8_ptr(i8* %arg3)
  call void @use_i8_ptr(i8* %arg4)
  call void @use_i8_ptr_ret(i8* %arg1)
  call void @use_i8_ptr_ret(i8* %arg2)
  call void @llvm.assume(i1 true) ["nofree"(i8* %arg1), "nofree"(i8* %arg4)]
  call void @use_i8_ptr_ret(i8* %arg3)
  call void @use_i8_ptr_ret(i8* %arg4)
  ret void
}
declare void @llvm.assume(i1)
declare void @unknown(i8*, i8*, i8*, i8*)
declare void @use_i8_ptr(i8* nocapture readnone) nounwind
declare void @use_i8_ptr_ret(i8* nocapture readnone) nounwind willreturn

declare noalias i8* @malloc(i64)

attributes #0 = { nounwind uwtable noinline }
attributes #1 = { nounwind }
attributes #2 = { nobuiltin nounwind }
