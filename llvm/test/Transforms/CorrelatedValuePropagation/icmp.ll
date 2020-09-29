; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -correlated-propagation -S %s | FileCheck %s
; RUN: opt -passes=correlated-propagation -S %s | FileCheck %s

target datalayout = "e-m:o-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.10.0"

declare void @check1(i1) #1
declare void @check2(i1) #1

; Make sure we propagate the value of %tmp35 to the true/false cases

define void @test1(i64 %tmp35) {
; CHECK-LABEL: @test1(
; CHECK-NEXT:  bb:
; CHECK-NEXT:    [[TMP36:%.*]] = icmp sgt i64 [[TMP35:%.*]], 0
; CHECK-NEXT:    br i1 [[TMP36]], label [[BB_TRUE:%.*]], label [[BB_FALSE:%.*]]
; CHECK:       bb_true:
; CHECK-NEXT:    tail call void @check1(i1 false) #0
; CHECK-NEXT:    unreachable
; CHECK:       bb_false:
; CHECK-NEXT:    tail call void @check2(i1 true) #0
; CHECK-NEXT:    unreachable
;
bb:
  %tmp36 = icmp sgt i64 %tmp35, 0
  br i1 %tmp36, label %bb_true, label %bb_false

bb_true:
  %tmp47 = icmp slt i64 %tmp35, 0
  tail call void @check1(i1 %tmp47) #4
  unreachable

bb_false:
  %tmp48 = icmp sle i64 %tmp35, 0
  tail call void @check2(i1 %tmp48) #4
  unreachable
}

; This is the same as test1 but with a diamond to ensure we
; get %tmp36 from both true and false BBs.

define void @test2(i64 %tmp35, i1 %inner_cmp) {
; CHECK-LABEL: @test2(
; CHECK-NEXT:  bb:
; CHECK-NEXT:    [[TMP36:%.*]] = icmp sgt i64 [[TMP35:%.*]], 0
; CHECK-NEXT:    br i1 [[TMP36]], label [[BB_TRUE:%.*]], label [[BB_FALSE:%.*]]
; CHECK:       bb_true:
; CHECK-NEXT:    br i1 [[INNER_CMP:%.*]], label [[INNER_TRUE:%.*]], label [[INNER_FALSE:%.*]]
; CHECK:       inner_true:
; CHECK-NEXT:    br label [[MERGE:%.*]]
; CHECK:       inner_false:
; CHECK-NEXT:    br label [[MERGE]]
; CHECK:       merge:
; CHECK-NEXT:    tail call void @check1(i1 false)
; CHECK-NEXT:    unreachable
; CHECK:       bb_false:
; CHECK-NEXT:    tail call void @check2(i1 true) #0
; CHECK-NEXT:    unreachable
;
bb:
  %tmp36 = icmp sgt i64 %tmp35, 0
  br i1 %tmp36, label %bb_true, label %bb_false

bb_true:
  br i1 %inner_cmp, label %inner_true, label %inner_false

inner_true:
  br label %merge

inner_false:
  br label %merge

merge:
  %tmp47 = icmp slt i64 %tmp35, 0
  tail call void @check1(i1 %tmp47) #0
  unreachable

bb_false:
  %tmp48 = icmp sle i64 %tmp35, 0
  tail call void @check2(i1 %tmp48) #4
  unreachable
}

; Make sure binary operator transfer functions are run when RHS is non-constant

define i1 @test3(i32 %x, i32 %y) #0 {
; CHECK-LABEL: @test3(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp ult i32 [[X:%.*]], 10
; CHECK-NEXT:    br i1 [[CMP1]], label [[CONT1:%.*]], label [[OUT:%.*]]
; CHECK:       cont1:
; CHECK-NEXT:    [[CMP2:%.*]] = icmp ult i32 [[Y:%.*]], 10
; CHECK-NEXT:    br i1 [[CMP2]], label [[CONT2:%.*]], label [[OUT]]
; CHECK:       cont2:
; CHECK-NEXT:    [[ADD:%.*]] = add nuw nsw i32 [[X]], [[Y]]
; CHECK-NEXT:    br label [[CONT3:%.*]]
; CHECK:       cont3:
; CHECK-NEXT:    br label [[OUT]]
; CHECK:       out:
; CHECK-NEXT:    ret i1 true
;
entry:
  %cmp1 = icmp ult i32 %x, 10
  br i1 %cmp1, label %cont1, label %out

cont1:
  %cmp2 = icmp ult i32 %y, 10
  br i1 %cmp2, label %cont2, label %out

cont2:
  %add = add i32 %x, %y
  br label %cont3

cont3:
  %cmp3 = icmp ult i32 %add, 25
  br label %out

out:
  %ret = phi i1 [ true, %entry], [ true, %cont1 ], [ %cmp3, %cont3 ]
  ret i1 %ret
}

; Same as previous but make sure nobody gets over-zealous

define i1 @test4(i32 %x, i32 %y) #0 {
; CHECK-LABEL: @test4(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp ult i32 [[X:%.*]], 10
; CHECK-NEXT:    br i1 [[CMP1]], label [[CONT1:%.*]], label [[OUT:%.*]]
; CHECK:       cont1:
; CHECK-NEXT:    [[CMP2:%.*]] = icmp ult i32 [[Y:%.*]], 10
; CHECK-NEXT:    br i1 [[CMP2]], label [[CONT2:%.*]], label [[OUT]]
; CHECK:       cont2:
; CHECK-NEXT:    [[ADD:%.*]] = add nuw nsw i32 [[X]], [[Y]]
; CHECK-NEXT:    br label [[CONT3:%.*]]
; CHECK:       cont3:
; CHECK-NEXT:    [[CMP3:%.*]] = icmp ult i32 [[ADD]], 15
; CHECK-NEXT:    br label [[OUT]]
; CHECK:       out:
; CHECK-NEXT:    [[RET:%.*]] = phi i1 [ true, [[ENTRY:%.*]] ], [ true, [[CONT1]] ], [ [[CMP3]], [[CONT3]] ]
; CHECK-NEXT:    ret i1 [[RET]]
;
entry:
  %cmp1 = icmp ult i32 %x, 10
  br i1 %cmp1, label %cont1, label %out

cont1:
  %cmp2 = icmp ult i32 %y, 10
  br i1 %cmp2, label %cont2, label %out

cont2:
  %add = add i32 %x, %y
  br label %cont3

cont3:
  %cmp3 = icmp ult i32 %add, 15
  br label %out

out:
  %ret = phi i1 [ true, %entry], [ true, %cont1 ], [ %cmp3, %cont3 ]
  ret i1 %ret
}

; Make sure binary operator transfer functions are run when RHS is non-constant

define i1 @test5(i32 %x, i32 %y) #0 {
; CHECK-LABEL: @test5(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp ult i32 [[X:%.*]], 5
; CHECK-NEXT:    br i1 [[CMP1]], label [[CONT1:%.*]], label [[OUT:%.*]]
; CHECK:       cont1:
; CHECK-NEXT:    [[CMP2:%.*]] = icmp ult i32 [[Y:%.*]], 5
; CHECK-NEXT:    br i1 [[CMP2]], label [[CONT2:%.*]], label [[OUT]]
; CHECK:       cont2:
; CHECK-NEXT:    [[SHIFTED:%.*]] = shl i32 [[X]], [[Y]]
; CHECK-NEXT:    br label [[CONT3:%.*]]
; CHECK:       cont3:
; CHECK-NEXT:    br label [[OUT]]
; CHECK:       out:
; CHECK-NEXT:    ret i1 true
;
entry:
  %cmp1 = icmp ult i32 %x, 5
  br i1 %cmp1, label %cont1, label %out

cont1:
  %cmp2 = icmp ult i32 %y, 5
  br i1 %cmp2, label %cont2, label %out

cont2:
  %shifted = shl i32 %x, %y
  br label %cont3

cont3:
  %cmp3 = icmp ult i32 %shifted, 65536
  br label %out

out:
  %ret = phi i1 [ true, %entry], [ true, %cont1 ], [ %cmp3, %cont3 ]
  ret i1 %ret
}

; Same as previous but make sure nobody gets over-zealous

define i1 @test6(i32 %x, i32 %y) #0 {
; CHECK-LABEL: @test6(
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[CMP1:%.*]] = icmp ult i32 [[X:%.*]], 5
; CHECK-NEXT:    br i1 [[CMP1]], label [[CONT1:%.*]], label [[OUT:%.*]]
; CHECK:       cont1:
; CHECK-NEXT:    [[CMP2:%.*]] = icmp ult i32 [[Y:%.*]], 15
; CHECK-NEXT:    br i1 [[CMP2]], label [[CONT2:%.*]], label [[OUT]]
; CHECK:       cont2:
; CHECK-NEXT:    [[SHIFTED:%.*]] = shl i32 [[X]], [[Y]]
; CHECK-NEXT:    br label [[CONT3:%.*]]
; CHECK:       cont3:
; CHECK-NEXT:    [[CMP3:%.*]] = icmp ult i32 [[SHIFTED]], 65536
; CHECK-NEXT:    br label [[OUT]]
; CHECK:       out:
; CHECK-NEXT:    [[RET:%.*]] = phi i1 [ true, [[ENTRY:%.*]] ], [ true, [[CONT1]] ], [ [[CMP3]], [[CONT3]] ]
; CHECK-NEXT:    ret i1 [[RET]]
;
entry:
  %cmp1 = icmp ult i32 %x, 5
  br i1 %cmp1, label %cont1, label %out

cont1:
  %cmp2 = icmp ult i32 %y, 15
  br i1 %cmp2, label %cont2, label %out

cont2:
  %shifted = shl i32 %x, %y
  br label %cont3

cont3:
  %cmp3 = icmp ult i32 %shifted, 65536
  br label %out

out:
  %ret = phi i1 [ true, %entry], [ true, %cont1 ], [ %cmp3, %cont3 ]
  ret i1 %ret
}

attributes #4 = { noreturn }
