; ModuleID = '/app/example.cpp'
source_filename = "/app/example.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [11 x i8] c"%d+%d: %d\0A\00", align 1
@.str.1 = private unnamed_addr constant [11 x i8] c"%d*%d: %d\0A\00", align 1
@.str.2 = private unnamed_addr constant [11 x i8] c"%d^%d: %d\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @_Z3addii(i32 %0, i32 %1) #0 !dbg !7 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  call void @llvm.dbg.declare(metadata i32* %3, metadata !12, metadata !DIExpression()), !dbg !13
  store i32 %1, i32* %4, align 4
  call void @llvm.dbg.declare(metadata i32* %4, metadata !14, metadata !DIExpression()), !dbg !15
  %5 = load i32, i32* %3, align 4, !dbg !16
  %6 = load i32, i32* %4, align 4, !dbg !17
  %7 = add nsw i32 %5, %6, !dbg !18
  ret i32 %7, !dbg !19
}

; Function Attrs: nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @_Z4multii(i32 %0, i32 %1) #0 !dbg !20 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  call void @llvm.dbg.declare(metadata i32* %3, metadata !21, metadata !DIExpression()), !dbg !22
  store i32 %1, i32* %4, align 4
  call void @llvm.dbg.declare(metadata i32* %4, metadata !23, metadata !DIExpression()), !dbg !24
  call void @llvm.dbg.declare(metadata i32* %5, metadata !25, metadata !DIExpression()), !dbg !26
  store i32 0, i32* %5, align 4, !dbg !26
  call void @llvm.dbg.declare(metadata i32* %6, metadata !27, metadata !DIExpression()), !dbg !29
  store i32 0, i32* %6, align 4, !dbg !29
  br label %7, !dbg !30

7:                                                ; preds = %15, %2
  %8 = load i32, i32* %6, align 4, !dbg !31
  %9 = load i32, i32* %4, align 4, !dbg !33
  %10 = icmp slt i32 %8, %9, !dbg !34
  br i1 %10, label %11, label %18, !dbg !35

11:                                               ; preds = %7
  %12 = load i32, i32* %3, align 4, !dbg !36
  %13 = load i32, i32* %5, align 4, !dbg !38
  %14 = call i32 @_Z3addii(i32 %12, i32 %13), !dbg !39
  store i32 %14, i32* %5, align 4, !dbg !40
  br label %15, !dbg !41

15:                                               ; preds = %11
  %16 = load i32, i32* %6, align 4, !dbg !42
  %17 = add nsw i32 %16, 1, !dbg !42
  store i32 %17, i32* %6, align 4, !dbg !42
  br label %7, !dbg !43, !llvm.loop !44

18:                                               ; preds = %7
  %19 = load i32, i32* %5, align 4, !dbg !46
  ret i32 %19, !dbg !47
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @_Z4powsii(i32 %0, i32 %1) #0 !dbg !48 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  call void @llvm.dbg.declare(metadata i32* %3, metadata !49, metadata !DIExpression()), !dbg !50
  store i32 %1, i32* %4, align 4
  call void @llvm.dbg.declare(metadata i32* %4, metadata !51, metadata !DIExpression()), !dbg !52
  call void @llvm.dbg.declare(metadata i32* %5, metadata !53, metadata !DIExpression()), !dbg !54
  store i32 1, i32* %5, align 4, !dbg !54
  call void @llvm.dbg.declare(metadata i32* %6, metadata !55, metadata !DIExpression()), !dbg !57
  store i32 0, i32* %6, align 4, !dbg !57
  br label %7, !dbg !58

7:                                                ; preds = %15, %2
  %8 = load i32, i32* %6, align 4, !dbg !59
  %9 = load i32, i32* %4, align 4, !dbg !61
  %10 = icmp slt i32 %8, %9, !dbg !62
  br i1 %10, label %11, label %18, !dbg !63

11:                                               ; preds = %7
  %12 = load i32, i32* %5, align 4, !dbg !64
  %13 = load i32, i32* %3, align 4, !dbg !66
  %14 = call i32 @_Z4multii(i32 %12, i32 %13), !dbg !67
  store i32 %14, i32* %5, align 4, !dbg !68
  br label %15, !dbg !69

15:                                               ; preds = %11
  %16 = load i32, i32* %6, align 4, !dbg !70
  %17 = add nsw i32 %16, 1, !dbg !70
  store i32 %17, i32* %6, align 4, !dbg !70
  br label %7, !dbg !71, !llvm.loop !72

18:                                               ; preds = %7
  %19 = load i32, i32* %5, align 4, !dbg !74
  ret i32 %19, !dbg !75
}

; Function Attrs: noinline norecurse optnone uwtable
define dso_local i32 @main() #2 !dbg !76 {
  %1 = call i32 @_Z3addii(i32 4, i32 2), !dbg !79
  %2 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str, i64 0, i64 0), i32 4, i32 2, i32 %1), !dbg !80
  %3 = call i32 @_Z4multii(i32 4, i32 8), !dbg !81
  %4 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.1, i64 0, i64 0), i32 4, i32 8, i32 %3), !dbg !82
  %5 = call i32 @_Z4powsii(i32 3, i32 4), !dbg !83
  %6 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.2, i64 0, i64 0), i32 3, i32 4, i32 %5), !dbg !84
  ret i32 0, !dbg !85
}

declare dso_local i32 @printf(i8*, ...) #3

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind readnone speculatable willreturn }
attributes #2 = { noinline norecurse optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 10.0.0 (https://github.com/llvm/llvm-project.git d32170dbd5b0d54436537b6b75beaf44324e0c28)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/app/example.cpp", directory: "/app")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 10.0.0 (https://github.com/llvm/llvm-project.git d32170dbd5b0d54436537b6b75beaf44324e0c28)"}
!7 = distinct !DISubprogram(name: "add", linkageName: "_Z3addii", scope: !8, file: !8, line: 3, type: !9, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!8 = !DIFile(filename: "example.cpp", directory: "/app")
!9 = !DISubroutineType(types: !10)
!10 = !{!11, !11, !11}
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DILocalVariable(name: "x", arg: 1, scope: !7, file: !8, line: 3, type: !11)
!13 = !DILocation(line: 3, column: 13, scope: !7)
!14 = !DILocalVariable(name: "y", arg: 2, scope: !7, file: !8, line: 3, type: !11)
!15 = !DILocation(line: 3, column: 20, scope: !7)
!16 = !DILocation(line: 4, column: 11, scope: !7)
!17 = !DILocation(line: 4, column: 15, scope: !7)
!18 = !DILocation(line: 4, column: 13, scope: !7)
!19 = !DILocation(line: 4, column: 4, scope: !7)
!20 = distinct !DISubprogram(name: "mult", linkageName: "_Z4multii", scope: !8, file: !8, line: 7, type: !9, scopeLine: 7, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!21 = !DILocalVariable(name: "x", arg: 1, scope: !20, file: !8, line: 7, type: !11)
!22 = !DILocation(line: 7, column: 14, scope: !20)
!23 = !DILocalVariable(name: "y", arg: 2, scope: !20, file: !8, line: 7, type: !11)
!24 = !DILocation(line: 7, column: 21, scope: !20)
!25 = !DILocalVariable(name: "result", scope: !20, file: !8, line: 8, type: !11)
!26 = !DILocation(line: 8, column: 8, scope: !20)
!27 = !DILocalVariable(name: "i", scope: !28, file: !8, line: 9, type: !11)
!28 = distinct !DILexicalBlock(scope: !20, file: !8, line: 9, column: 4)
!29 = !DILocation(line: 9, column: 13, scope: !28)
!30 = !DILocation(line: 9, column: 9, scope: !28)
!31 = !DILocation(line: 9, column: 20, scope: !32)
!32 = distinct !DILexicalBlock(scope: !28, file: !8, line: 9, column: 4)
!33 = !DILocation(line: 9, column: 24, scope: !32)
!34 = !DILocation(line: 9, column: 22, scope: !32)
!35 = !DILocation(line: 9, column: 4, scope: !28)
!36 = !DILocation(line: 10, column: 15, scope: !37)
!37 = distinct !DILexicalBlock(scope: !32, file: !8, line: 9, column: 32)
!38 = !DILocation(line: 10, column: 18, scope: !37)
!39 = !DILocation(line: 10, column: 11, scope: !37)
!40 = !DILocation(line: 10, column: 9, scope: !37)
!41 = !DILocation(line: 11, column: 4, scope: !37)
!42 = !DILocation(line: 9, column: 28, scope: !32)
!43 = !DILocation(line: 9, column: 4, scope: !32)
!44 = distinct !{!44, !35, !45}
!45 = !DILocation(line: 11, column: 4, scope: !28)
!46 = !DILocation(line: 12, column: 11, scope: !20)
!47 = !DILocation(line: 12, column: 4, scope: !20)
!48 = distinct !DISubprogram(name: "pows", linkageName: "_Z4powsii", scope: !8, file: !8, line: 15, type: !9, scopeLine: 15, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!49 = !DILocalVariable(name: "x", arg: 1, scope: !48, file: !8, line: 15, type: !11)
!50 = !DILocation(line: 15, column: 14, scope: !48)
!51 = !DILocalVariable(name: "y", arg: 2, scope: !48, file: !8, line: 15, type: !11)
!52 = !DILocation(line: 15, column: 21, scope: !48)
!53 = !DILocalVariable(name: "result", scope: !48, file: !8, line: 16, type: !11)
!54 = !DILocation(line: 16, column: 8, scope: !48)
!55 = !DILocalVariable(name: "i", scope: !56, file: !8, line: 17, type: !11)
!56 = distinct !DILexicalBlock(scope: !48, file: !8, line: 17, column: 4)
!57 = !DILocation(line: 17, column: 13, scope: !56)
!58 = !DILocation(line: 17, column: 9, scope: !56)
!59 = !DILocation(line: 17, column: 20, scope: !60)
!60 = distinct !DILexicalBlock(scope: !56, file: !8, line: 17, column: 4)
!61 = !DILocation(line: 17, column: 24, scope: !60)
!62 = !DILocation(line: 17, column: 22, scope: !60)
!63 = !DILocation(line: 17, column: 4, scope: !56)
!64 = !DILocation(line: 18, column: 20, scope: !65)
!65 = distinct !DILexicalBlock(scope: !60, file: !8, line: 17, column: 32)
!66 = !DILocation(line: 18, column: 28, scope: !65)
!67 = !DILocation(line: 18, column: 15, scope: !65)
!68 = !DILocation(line: 18, column: 13, scope: !65)
!69 = !DILocation(line: 19, column: 4, scope: !65)
!70 = !DILocation(line: 17, column: 28, scope: !60)
!71 = !DILocation(line: 17, column: 4, scope: !60)
!72 = distinct !{!72, !63, !73}
!73 = !DILocation(line: 19, column: 4, scope: !56)
!74 = !DILocation(line: 20, column: 11, scope: !48)
!75 = !DILocation(line: 20, column: 4, scope: !48)
!76 = distinct !DISubprogram(name: "main", scope: !8, file: !8, line: 24, type: !77, scopeLine: 24, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!77 = !DISubroutineType(types: !78)
!78 = !{!11}
!79 = !DILocation(line: 25, column: 32, scope: !76)
!80 = !DILocation(line: 25, column: 4, scope: !76)
!81 = !DILocation(line: 26, column: 32, scope: !76)
!82 = !DILocation(line: 26, column: 4, scope: !76)
!83 = !DILocation(line: 27, column: 32, scope: !76)
!84 = !DILocation(line: 27, column: 4, scope: !76)
!85 = !DILocation(line: 28, column: 1, scope: !76)