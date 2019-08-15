// XD Pascal - a 32-bit compiler for Windows
// Developed by Vasiliy Tereshkov, 2009-2010, 2019

// Code generator



procedure Gen(b: Byte);
begin
if CodeSize >= MAXCODESIZE then
  Error('Maximum code size exceeded', '', -1);
Code[CodeSize] := b;
Inc(CodeSize);
end;




procedure GenAt(Pos: LongInt; b: Byte);
begin
Code[Pos] := b;
end;




procedure GenWord(w: Integer);
var
  i: Integer;
begin
for i := 1 to 2 do
  begin
  Gen(Byte(w and $FF));
  w := w shr 8;
  end;
end;




procedure GenWordAt(Pos: LongInt; w: Integer);
var
  i: Integer;
begin
for i := 0 to 1 do
  begin
  GenAt(Pos + i, Byte(w and $FF));
  w := w shr 8;
  end;
end;




procedure GenDWord(dw: LongInt);
var
  i: Integer;
begin
for i := 1 to 4 do
  begin
  Gen(Byte(dw and $FF));
  dw := dw shr 8;
  end;
end;




procedure GenDWordAt(Pos: LongInt; dw: LongInt);
var
  i: Integer;
begin
for i := 0 to 3 do
  begin
  GenAt(Pos + i, Byte(dw and $FF));
  dw := dw shr 8;
  end;
end;




procedure GenRelocDWord(dw: LongInt; RelocType: Byte);
begin
Inc(NumRelocs);
if NumRelocs > MAXRELOCS then
  Error('Maximum number of relocations exceeded', '', -1);  
Reloc[NumRelocs].RelocType := RelocType;
Reloc[NumRelocs].Pos := CodeSize;
Reloc[NumRelocs].Value := dw;

GenDWord(dw);
end;




procedure PushConst(Value: LongInt);
begin
Gen($68); GenDWord(Value);                            // push Value
end;




procedure PushRelocConst(Value: LongInt; RelocType: Byte);
begin
Gen($68); GenRelocDWord(Value, RelocType);                        // push Value  ; relocatable
end;




procedure Relocate(CodeDeltaAddr, DataDeltaAddr: Integer);
var
  i, DeltaAddr: Integer;
begin
for i := 1 to NumRelocs do
  begin
  case Reloc[i].RelocType of
    RELOCCODE: DeltaAddr := CodeDeltaAddr;
    RELOCDATA: DeltaAddr := DataDeltaAddr
  else 
    DeltaAddr := 0;
  end;  
  
  GenDWordAt(Reloc[i].Pos, Reloc[i].Value + DeltaAddr);
  end;
end;  




procedure PushVarPtr(Addr: Integer; Scope: Byte; DeltaNesting: Byte; RelocType: Byte);
const
  StaticLinkAddr = 2 * 4;
var
  i: Integer;  
begin
case Scope of
  GLOBAL:                                     // Global variable
    PushRelocConst(Addr, RelocType);
  LOCAL:
    begin
    if DeltaNesting = 0 then                  // Strictly local variable
      begin
      Gen($8D); Gen($85); GenDWord(Addr);                       // lea eax, [ebp + Addr]
      end
    else                                      // Intermediate level variable
      begin
      Gen($8B); Gen($75); Gen(StaticLinkAddr);                  // mov esi, [ebp + StaticLinkAddr]
      for i := 1 to DeltaNesting - 1 do
        begin
        Gen($8B); Gen($76); Gen(StaticLinkAddr);                // mov esi, [esi + StaticLinkAddr]
        end;
      Gen($8D); Gen($86); GenDWord(Addr);                       // lea eax, [esi + Addr]
      end;
    Gen($50);                                                   // push eax
    end;// if
end;// case
end;




procedure DerefPtr(DataType: Byte);
begin
Gen($5E);                                                       // pop esi

case TypeSize(DataType) of
  1: begin
     Gen($8A); Gen($06);                                        // mov al, [esi]
     Gen($66); Gen($98);                                        // cbw
     Gen($98);                                                  // cwde
     end;
  2: begin
     Gen($66); Gen($8B); Gen($06);                              // mov ax, [esi]
     Gen($98);                                                  // cwde
     end;
  4: begin
     Gen($8B); Gen($06);                                        // mov eax, [esi]
     end
else
  Error('Internal fault: Illegal designator size', '', -1);
end;

Gen($50);                                                       // push eax
end;




procedure GetArrayElementPtr(ArrType: Byte);

  function Log2(x: LongInt): ShortInt;
  var
    i: Integer;
  begin
  Result := -1;
  i := 0;
  while (i <= 31) and (Result = -1) do
    begin
    if x = 1 shl i then Result := i;
    Inc(i);
    end;
  end;

var
  BaseTypeSize, IndexLowBound: Integer;
  Log2BaseTypeSize: ShortInt;

begin
Gen($58);                                                       // pop eax           ; Array index

IndexLowBound := LowBound(Types[ArrType].IndexType);
if IndexLowBound = 1 then
  Gen($48)                                                      // dec eax
else if IndexLowBound <> 0 then
  begin
  Gen($2D); GenDWord(IndexLowBound);                            // sub eax, IndexLowBound
  end;

BaseTypeSize := TypeSize(Types[ArrType].BaseType);
Log2BaseTypeSize := Log2(BaseTypeSize);

if Log2BaseTypeSize > 0 then
  begin
  Gen($C1); Gen($E0); Gen(Log2BaseTypeSize);                    // shl eax, Log2BaseTypeSize
  end
else if Log2BaseTypeSize < 0 then
  begin
  Gen($B9); GenDWord(BaseTypeSize);                             // mov ecx, BaseTypeSize
  Gen($F7); Gen($E1);                                           // mul ecx
  end;

Gen($5B);                                                       // pop ebx            ; Array base offset
Gen($03); Gen($D8);                                             // add ebx, eax
Gen($53);                                                       // push ebx
end;




procedure GetFieldPtr(RecType: Byte; FieldIndex: Integer);
var
  Offset: Integer;
begin
Offset := Types[RecType].Field[FieldIndex]^.Offset;
if Offset > 0 then
  begin
  Gen($58);                                                       // pop eax
  Gen($05); GenDWord(Offset);                                     // add eax, Offset
  Gen($50);                                                       // push eax
  end;
end;




procedure SaveStackTopToEAX;
begin
Gen($58);                                                         // pop eax
end;




procedure RestoreStackTopFromEAX;
begin
Gen($50);                                                         // push eax
end;




procedure SaveStackTopToEDX;
begin
Gen($5A);                                                         // pop edx
end;




procedure RestoreStackTopFromEDX;
begin
Gen($52);                                                         // push edx
end;





procedure SaveFileHandle;
begin
Gen($5F);                                                   // pop edi
Gen($58);                                                   // pop eax  ; pop and discard unnecessary stream handle, i.e. 0
Gen($58);                                                   // pop eax  ; pop and discard unnecessary console handle, i.e. 0
end;




procedure RestoreFileHandle;
begin
Gen($57);                                                   // push edi
end;




procedure GenerateIncDec(proc, Size: Byte);
begin
Gen($5E);                                                             // pop esi

case Size of
  1: begin
     Gen($FE);                                                        // ... byte ptr ...
     end;
  2: begin
     Gen($66); Gen($FF);                                              // ... word ptr ...
     end;
  4: begin
     Gen($FF);                                                        // ... dword ptr ...
     end;
  end;

case proc of
  INCPROC: Gen($06);                                                  // inc ... [esi]
  DECPROC: Gen($0E);                                                  // dec ... [esi]
  end;
end;




procedure GenerateRound(TruncMode: Boolean);
begin
Gen($89); Gen($E3);                                                   // mov ebx, esp
Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]   ;  st := operand
if TruncMode then
  begin
  Gen($66); Gen($C7); Gen($43); Gen(Byte(-4)); GenWord($0F7F);        // mov word ptr [ebx - 4], 0F7Fh
  Gen($D9); Gen($6B); Gen(Byte(-4));                                  // fldcw word ptr [ebx - 4]
  end;
Gen($DB); Gen($1B);                                                   // fistp dword ptr [ebx] ;  [ebx] := round(result);  pop
if TruncMode then
  begin
  Gen($66); Gen($C7); Gen($43); Gen(Byte(-4)); GenWord($037F);        // mov word ptr [ebx - 4], 037Fh
  Gen($D9); Gen($6B); Gen(Byte(-4));                                  // fldcw word ptr [ebx - 4]
  end;
end;// GenerateRound




procedure GenerateFloat(Depth: Byte);
begin
Gen($89); Gen($E3);                                                   // mov ebx, esp

if Depth > 0 then
  begin
  Gen($83); Gen($C3); Gen(Depth);                                     // add ebx, Depth
  end;

Gen($DB); Gen($03);                                                   // fild dword ptr [ebx]  ;  st := float(operand)
Gen($D9); Gen($1B);                                                   // fstp dword ptr [ebx]  ;  [ebx] := result;  pop
end;// GenerateFloat




procedure GenerateMathFunction(func, ResultType: Byte);
begin
if Types[ResultType].TypeKind = REALTYPE then       // Real type
  begin
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]  ; st = x
  case func of
    ABSFUNC:
      begin
      Gen($D9); Gen($E1);                                               // fabs
      end;
    SQRFUNC:
      begin
      Gen($DC); Gen($C8);                                               // fmul st, st
      end;
    SINFUNC:
      begin
      Gen($D9); Gen($FE);                                               // fsin
      end;
    COSFUNC:
      begin
      Gen($D9); Gen($FF);                                               // fcos
      end;
    ARCTANFUNC:
      begin
      Gen($D9); Gen($E8);                                               // fld1
      Gen($D9); Gen($F3);                                               // fpatan    ; st := arctan(x / 1.0)
      end;
    EXPFUNC:
      begin
      Gen($D9); Gen($EA);                                               // fldl2e
      Gen($DE); Gen($C9);                                               // fmul
      Gen($D9); Gen($C0);                                               // fld st
      Gen($D9); Gen($FC);                                               // frndint
      Gen($DD); Gen($D2);                                               // fst st(2) ; st(2) := round(x * log2(e))
      Gen($DE); Gen($E9);                                               // fsub
      Gen($D9); Gen($F0);                                               // f2xm1     ; st := 2 ^ frac(x * log2(e)) - 1
      Gen($D9); Gen($E8);                                               // fld1
      Gen($DE); Gen($C1);                                               // fadd
      Gen($D9); Gen($FD);                                               // fscale    ; st := 2 ^ frac(x * log2(e)) * 2 ^ round(x * log2(e)) = exp(x)
      end;
    LNFUNC:
      begin
      Gen($D9); Gen($ED);                                               // fldln2
      Gen($D9); Gen($C9);                                               // fxch
      Gen($D9); Gen($F1);                                               // fyl2x     ; st := ln(2) * log2(x) = ln(x)
      end;
    SQRTFUNC:
      begin
      Gen($D9); Gen($FA);                                               // fsqrt
      end;

  end;// case

  Gen($D9); Gen($1B);                                                   // fstp dword ptr [ebx]  ;  [ebx] := result;  pop
  end
else                                // Ordinal types
  case func of
    ABSFUNC:
      begin
      Gen($58);                                                         // pop eax
      Gen($83); Gen($F8); Gen($00);                                     // cmp eax, 0
      Gen($7D); Gen($02);                                               // jge +2
      Gen($F7); Gen($D8);                                               // neg eax
      Gen($50);                                                         // push eax
      end;
    SQRFUNC:
      begin
      Gen($58);                                                         // pop eax
      Gen($F7); Gen($E8);                                               // imul eax
      Gen($50);                                                         // push eax
      end;
  end;// case
end;// GenerateMathFunction





procedure GenerateUnaryOperator(op: Byte; ResultType: Byte);
begin
if Types[ResultType].TypeKind = REALTYPE then     // Real type
  begin
  if op = MINUSTOK then
    begin
    Gen($89); Gen($E3);                                                 // mov ebx, esp
    Gen($D9); Gen($03);                                                 // fld dword ptr [ebx]  ; st = x
    Gen($D9); Gen($E0);                                                 // fchs
    Gen($D9); Gen($1B);                                                 // fstp dword ptr [ebx]  ;  [ebx] := result;  pop
    end;
  end
else                                              // Ordinal types
  begin
  Gen($58);                                                             // pop eax
  case op of
    MINUSTOK:
      begin
      Gen($F7); Gen($D8);                                               // neg eax
      end;
    NOTTOK:
      begin
      Gen($F7); Gen($D0);                                               // not eax
      end;
  end;// case
  Gen($50);                                                             // push eax
  end;// else
  
end;




procedure GenerateBinaryOperator(op: Byte; ResultType: Byte);
begin
if Types[ResultType].TypeKind = REALTYPE then     // Real type
  begin
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]  ; st = operand2
  Gen($58);                                                             // pop eax
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]  ; st(1) = operand2;  st = operand1
  Gen($D9); Gen($C9);                                                   // fxch                 ; st = operand2;  st(1) = operand1

  case op of
    PLUSTOK:
      begin
      Gen($DE); Gen($C1);                                               // fadd  ;  st(1) := st(1) + st;  pop
      end;
    MINUSTOK:
      begin
      Gen($DE); Gen($E9);                                               // fsub  ;  st(1) := st(1) - st;  pop
      end;
    MULTOK:
      begin
      Gen($DE); Gen($C9);                                               // fmul  ;  st(1) := st(1) * st;  pop
      end;
    DIVTOK:
      begin
      Gen($DE); Gen($F9);                                               // fdiv  ;  st(1) := st(1) / st;  pop
      end;
  end;// case

  Gen($D9); Gen($1B);                                                   // fstp dword ptr [ebx]  ;  [ebx] := result;  pop

  end // if
else                              // Ordinal types
  begin
  Gen($59);                                                             // pop ecx
  Gen($58);                                                             // pop eax

  case op of
    PLUSTOK:
      begin
      Gen($03); Gen($C1);                                               // add eax, ecx
      end;
    MINUSTOK:
      begin
      Gen($2B); Gen($C1);                                               // sub eax, ecx
      end;
    MULTOK:
      begin
      Gen($F7); Gen($E9);                                               // imul ecx
      end;
    IDIVTOK, MODTOK:
      begin
      Gen($99);                                                         // cdq
      Gen($F7); Gen($F9);                                               // idiv ecx
      if op = MODTOK then
        begin
        Gen($8B); Gen($C2);                                             // mov eax, edx         ; save remainder
        end;
      end;
    SHLTOK:
      begin
      Gen($D3); Gen($E0);                                               // shl eax, cl
      end;
    SHRTOK:
      begin
      Gen($D3); Gen($E8);                                               // shr eax, cl
      end;
    ANDTOK:
      begin
      Gen($23); Gen($C1);                                               // and eax, ecx
      end;
    ORTOK:
      begin
      Gen($0B); Gen($C1);                                               // or eax, ecx
      end;
    XORTOK:
      begin
      Gen($33); Gen($C1);                                               // xor eax, ecx
      end;

  end;// case

  Gen($50);                                                             // push eax
  end;// else
end;




procedure GenerateRelation(rel: Byte; ValType: Byte);
begin
if Types[ValType].TypeKind = REALTYPE then        // Real type
  begin
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]  ; st = operand2
  Gen($58);                                                             // pop eax
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($D9); Gen($03);                                                   // fld dword ptr [ebx]  ; st(1) = operand2;  st = operand1
  Gen($58);                                                             // pop eax
  Gen($89); Gen($E3);                                                   // mov ebx, esp
  Gen($DE); Gen($D9);                                                   // fcompp         ;  test st - st(1)
  Gen($DF); Gen($E0);                                                   // fnstsw ax
  Gen($68); GenDWord(-1);                                               // push FFFFFFFFh ;  TRUE
  Gen($9E);                                                             // sahf
  case rel of
    EQTOK: Gen($74);                                                    // je  ...
    NETOK: Gen($75);                                                    // jne ...
    GTTOK: Gen($77);                                                    // ja  ...
    GETOK: Gen($73);                                                    // jae ...
    LTTOK: Gen($72);                                                    // jb  ...
    LETOK: Gen($76);                                                    // jbe ...
  end;// case
  end
else                              // Ordinal types
  begin
  Gen($59);                                                             // pop ecx
  Gen($58);                                                             // pop eax
  Gen($68); GenDWord(-1);                                               // push FFFFFFFFh ;  TRUE
  Gen($3B); Gen($C1);                                                   // cmp eax, ecx
  case rel of
    EQTOK: Gen($74);                                                    // je  ...
    NETOK: Gen($75);                                                    // jne ...
    GTTOK: Gen($7F);                                                    // jg  ...
    GETOK: Gen($7D);                                                    // jge ...
    LTTOK: Gen($7C);                                                    // jl  ...
    LETOK: Gen($7E);                                                    // jle ...
  end;// case
  end;// else

Gen($06);                                                               // ... +6
Gen($59);                                                               // pop ecx
Gen($68); GenDWord(0);                                                  // push 00000000h ;  FALSE
end;





procedure GenerateAssignment(DesignatorType: Byte);
begin
// EDX should be preserved

// Source value
Gen($58);                                                               // pop eax
// Destination address
Gen($5E);                                                               // pop esi

case TypeSize(DesignatorType) of
  1: begin
     Gen($88); Gen($06);                                                // mov [esi], al
     end;
  2: begin
     Gen($66); Gen($89); Gen($06);                                      // mov [esi], ax
     end;
  4: begin
     Gen($89); Gen($06);                                                // mov [esi], eax
     end
else
  Error('Internal fault: Illegal designator size', '', -1);
end;

end;




procedure GenerateStructuredAssignment(DesignatorType: Byte);
begin
Gen($89); Gen($FB);                                                     // mov ebx, edi   ; edi is used in Write, Read, etc. and should be preserved

// Source address
Gen($5E);                                                               // pop esi
// Destination address
Gen($5F);                                                               // pop edi

// Copy source to destination
Gen($B9); GenDWord(TypeSize(DesignatorType));                           // mov ecx, TypeSize(DesignatorType)
Gen($FC);                                                               // cld          ; increment si, di after each step
Gen($F3); Gen($A4);                                                     // rep movsb

Gen($89); Gen($DF);                                                     // mov edi, ebx
end;




procedure GenerateCall(EntryPoint: LongInt; CallerNesting, CalleeNesting: Integer);
const
  StaticLinkAddr = 2 * 4;
var
  CodePos: Integer;
  i: Integer;
begin
if (CallerNesting < 0) or (CalleeNesting < 1) or (CallerNesting - CalleeNesting < -1) then
  Error('Internal fault: Illegal nesting level', '', -1);
  
if CalleeNesting > 1 then                        // If a nested routine is called, push static link as the last hidden parameter
  if CallerNesting - CalleeNesting = -1 then     // The caller and the callee's enclosing routine are at the same nesting level
    begin
    Gen($55);                                                             // push ebp
    end
  else                                           // The caller is deeper
    begin
    Gen($8B); Gen($75); Gen(StaticLinkAddr);                              // mov esi, [ebp + StaticLinkAddr]
    for i := 1 to CallerNesting - CalleeNesting do
      begin
      Gen($8B); Gen($76); Gen(StaticLinkAddr);                            // mov esi, [esi + StaticLinkAddr]
      end;
    Gen($56);                                                             // push esi
    end;

// Call the routine  
CodePos := CodeSize;
Gen($E8); GenDWord(EntryPoint - (CodePos + 5));                           // call EntryPoint
end;





procedure InverseStack(Depth: Integer);
var
  i: Integer;
begin
for i := 0 to Depth div 2 - 1 do
  begin
  Gen($8B); Gen($84); Gen($24); GenDWord(SizeOf(LongInt) * i);                        // mov eax, [esp + 4 * i]
  Gen($8B); Gen($9C); Gen($24); GenDWord(SizeOf(LongInt) * (Depth - i - 1));          // mov ebx, [esp + 4 * (Depth - i - 1)]
  Gen($89); Gen($84); Gen($24); GenDWord(SizeOf(LongInt) * (Depth - i - 1));          // mov [esp + 4 * (Depth - i - 1)], eax
  Gen($89); Gen($9C); Gen($24); GenDWord(SizeOf(LongInt) * i);                        // mov [esp + 4 * i], ebx  
  end;
end;





procedure GenerateExternalCall(EntryPoint: LongInt; NumParams: Integer; ReturnResult: Boolean);
begin
InverseStack(NumParams);
Gen($FF); Gen($15); GenDWord(EntryPoint);                               // call ds:[EntryPoint]
if ReturnResult then
  Gen($50);                                                             // push eax
end;




procedure GenerateReturn(TotalParamsSize, Nesting: Integer);
begin
Gen($C2);                                                               // ret ... 
if Nesting = 1 then
  GenWord(TotalParamsSize)                                              // ... TotalParamsSize
else  
  GenWord(TotalParamsSize + 4);                                         // ... TotalParamsSize + 4   ; + 4 is for static link
end;




procedure GenerateIfCondition;
begin
Gen($58);                                                   // pop eax
Gen($83); Gen($F8); Gen($00);                               // cmp eax, 0
Gen($75); Gen($05);                                         // jne +5
end;




procedure GenerateWhileCondition;
begin
GenerateIfCondition;
end;



procedure GenerateRepeatCondition;
begin
GenerateIfCondition;
end;




procedure GenerateForCondition(CounterAddress: Integer; Scope, CounterSize: Byte; Down: Boolean);
begin
Gen($59);                                                   // pop ecx
Gen($51);                                                   // push ecx             ; The final value of the counter will be removed from stack by GenerateForEpilog  
case Scope of
  GLOBAL:
    begin
    case CounterSize of
      1: begin
         Gen($A0);                                          // mov al, [...]
         end;
      2: begin
         Gen($66); Gen($A1);                                // mov ax, [...]
         end;
      4: begin
         Gen($A1);                                          // mov eax, [...]
         end;
      end;
    GenRelocDWord(CounterAddress, RELOCDATA);               // ... CounterAddress ...  ; relocatable
    end; 

  LOCAL:
    begin
    case CounterSize of
      1: begin
         Gen($8A); Gen($85);                                // mov al, [ebp + ...]
         end;
      2: begin
         Gen($66); Gen($8B); Gen($85);                      // mov ax, [ebp + ...]
         end;
      4: begin
         Gen($8B); Gen($85);                                // mov eax, [ebp + ...]
         end;
      end;
    GenDWord(CounterAddress);                               // ... CounterAddress ...  
    end;
    
  end;  
    
if CounterSize < 2 then
  begin
  Gen($66); Gen($98);                                       // cbw
  end;                                        
if CounterSize < 4 then
  begin
  Gen($98);                                                 // cwde
  end;

Gen($3B); Gen($C1);                                         // cmp eax, ecx
if Down then
  begin
  Gen($7D); Gen($05);                                       // jge +5
  end
else
  begin
  Gen($7E); Gen($05);                                       // jle +5
  end;
end;




procedure GenerateIfProlog;
begin
Inc(CodePosStackTop);
CodePosStack[CodePosStackTop] := CodeSize;

Gen($90);                                                   // nop   ; jump to the IF block end will be inserted here
Gen($90);                                                   // nop
Gen($90);                                                   // nop
Gen($90);                                                   // nop
Gen($90);                                                   // nop
end;




procedure GenerateElseProlog;
var
  CodePos: Integer;
begin
CodePos := CodePosStack[CodePosStackTop];
Dec(CodePosStackTop);

GenAt(CodePos, $E9); GenDWordAt(CodePos + 1, CodeSize - (CodePos + 5) + 5);  // jmp (IF..THEN block end)

GenerateIfProlog;
end;




procedure GenerateIfElseEpilog;
var
  CodePos: Integer;
begin
CodePos := CodePosStack[CodePosStackTop];
Dec(CodePosStackTop);

GenAt(CodePos, $E9); GenDWordAt(CodePos + 1, CodeSize - (CodePos + 5));      // jmp (IF..THEN block end)
end;




procedure GenerateCaseProlog;
begin
Gen($59);                                                   // pop ecx           ; CASE switch value
Gen($B0); Gen($00);                                         // mov al, 00h       ; initial flag mask
end;




procedure GenerateCaseEpilog(NumCaseStatements: Integer);
var
  i: Integer;
begin
for i := 1 to NumCaseStatements do
  GenerateIfElseEpilog;
end;




procedure GenerateCaseEqualityCheck(Value: LongInt);
begin
Gen($81); Gen($F9); GenDWord(Value);                        // cmp ecx, Value
Gen($9F);                                                   // lahf
Gen($0A); Gen($C4);                                         // or al, ah
end;




procedure GenerateCaseRangeCheck(Value1, Value2: LongInt);
begin
Gen($81); Gen($F9); GenDWord(Value1);                       // cmp ecx, Value1
Gen($7C); Gen($0A);                                         // jl +10
Gen($81); Gen($F9); GenDWord(Value2);                       // cmp ecx, Value2
Gen($7F); Gen($02);                                         // jg +2
Gen($0C); Gen($40);                                         // or al, 40h     ; set zero flag on success
end;




procedure GenerateCaseStatementProlog;
begin
Gen($24); Gen($40);                                         // and al, 40h    ; test zero flag
Gen($75); Gen($05);                                         // jnz +5         ; if set, jump to the case statement
GenerateIfProlog;
end;




procedure GenerateCaseStatementEpilog;
var
  StoredCodeSize: LongInt;
begin
StoredCodeSize := CodeSize;

Gen($90);                                                   // nop   ; jump to the CASE block end will be inserted here
Gen($90);                                                   // nop
Gen($90);                                                   // nop
Gen($90);                                                   // nop
Gen($90);                                                   // nop

GenerateIfElseEpilog;

Inc(CodePosStackTop);
CodePosStack[CodePosStackTop] := StoredCodeSize;
end;




procedure GenerateWhileEpilog;
var
  CodePos, CurPos, ReturnPos: Integer;
begin
CodePos := CodePosStack[CodePosStackTop];
Dec(CodePosStackTop);

GenAt(CodePos, $E9); GenDWordAt(CodePos + 1, CodeSize - (CodePos + 5) + 5);  // jmp (WHILE..DO block end)

ReturnPos := CodePosStack[CodePosStackTop];
Dec(CodePosStackTop);

CurPos := CodeSize;

Gen($E9); GenDWord(ReturnPos - (CurPos + 5));                                // jmp ReturnPos
end;




procedure GenerateRepeatProlog;
begin
Inc(CodePosStackTop);
CodePosStack[CodePosStackTop] := CodeSize;
end;




procedure GenerateRepeatEpilog;
var
  CurPos, ReturnPos: Integer;
begin
ReturnPos := CodePosStack[CodePosStackTop];
Dec(CodePosStackTop);

CurPos := CodeSize;

Gen($E9); GenDWord(ReturnPos - (CurPos + 5));                // jmp ReturnPos
end;




procedure GenerateForEpilog(CounterAddress: Integer; Scope, CounterSize: Byte; Down: Boolean);
begin
PushVarPtr(CounterAddress, Scope, 0, RELOCDATA);
if Down then
  GenerateIncDec(DECPROC, CounterSize)
else
  GenerateIncDec(INCPROC, CounterSize);
GenerateWhileEpilog;

Gen($59);                                                      // pop ecx    ; Remove the final value of the counter from the stack
end;




procedure GenerateNestedProcsProlog;
begin
GenerateIfProlog;
end;




procedure GenerateNestedProcsEpilog;
begin
GenerateIfElseEpilog;
end;




procedure GenerateProgramProlog;
begin
// Initialize FPU
Gen($DB); Gen($E3);                                         // finit
end;




procedure GenerateProgramEpilog;
begin

end;




procedure GenerateStackFrameProlog(TotalLocalVarsSize: Integer);
begin
Gen($55);                                                     // push ebp
Gen($8B); Gen($EC);                                           // mov ebp, esp
Gen($81); Gen($EC); GenDWord(TotalLocalVarsSize);             // sub esp, TotalLocalVarsSize
end;





procedure GenerateStackFrameEpilog;
begin
Gen($8B); Gen($E5);                                           // mov esp, ebp
Gen($5D);                                                     // pop ebp
end;




procedure GenerateForwardReference;
begin
Gen($90);                                                     // nop   ; jump to the procedure entry point will be inserted here
Gen($90);                                                     // nop
Gen($90);                                                     // nop
Gen($90);                                                     // nop
Gen($90);                                                     // nop
end;




procedure GenerateForwardResolution(IdentIndex: Integer);
var
  CodePos: Integer;
begin
CodePos := Ident[IdentIndex].Value;
GenAt(CodePos, $E9); GenDWordAt(CodePos + 1, CodeSize - (CodePos + 5));      // jmp Ident[IdentIndex].Value
end;
