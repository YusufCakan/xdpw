// System library



const
  pi = 3.1415927;
 
  
  // Windows API constants
  
  STD_INPUT_HANDLE      = -10;
  STD_OUTPUT_HANDLE     = -11;
  
  FILE_ATTRIBUTE_NORMAL = 128;
  
  CREATE_ALWAYS         = 2;
  OPEN_EXISTING         = 3;
  
  GENERIC_READ          = $80000000;
  GENERIC_WRITE         = $40000000;
  
  INVALID_HANDLE_VALUE  = -1;
  
  FILE_BEGIN            = 0;
  FILE_CURRENT          = 1;
  FILE_END              = 2;



type
  Byte = ShortInt;
  Word = SmallInt;
  LongInt = Integer;  
  Single = Real;
  file = Text;  
  PChar = ^Char;
  
  TStream = record
    Data: PChar;
    Index: Integer;
  end;

  PStream = ^TStream;  
  


var
  RandSeed: Integer;
  
  Heap: LongInt;
  
  Buffer: string;
  IOError: Integer;
  LastReadChar: Char;
  StdInputHandle, StdOutputHandle: Text;
  
  
  
// Windows API functions

function GetCommandLineA: Pointer; external 'GetCommandLineA';

function GetProcessHeap: LongInt; external 'GetProcessHeap';

function HeapAlloc(dwBytes, dwFlags, hHeap: LongInt): Pointer; external 'HeapAlloc';

procedure HeapFree(lpMem: Pointer; dwFlags, hHeap: LongInt); external 'HeapFree';

function GetStdHandle(nStdHandle: Integer): Text; external 'GetStdHandle';

procedure SetConsoleMode(dwMode: LongInt; hConsoleHandle: Text); external 'SetConsoleMode';

function CreateFileA(hTemplateFile, dwFlagsAndAttributes, dwCreationDisposition: LongInt; 
                     lpSecurityAttributes: Pointer; dwShareMode, dwDesiredAccess: LongInt; 
                     const lpFileName: string): Text; external 'CreateFileA';
                     
function SetFilePointer(dwMoveMethod: LongInt; lpDistanceToMoveHigh: Pointer; 
                        lDistanceToMove: LongInt; hFile: Text): LongInt; external 'SetFilePointer';

function GetFileSize(lpFileSizeHigh: Pointer; hFile: Text): LongInt; external 'GetFileSize';        
                     
procedure WriteFile(lpOverlapped: LongInt; var lpNumberOfBytesWritten: LongInt; nNumberOfBytesToWrite: LongInt; 
                    lpBuffer: Pointer; hFile: Text); external 'WriteFile';
                    
procedure ReadFile(lpOverlapped: LongInt; var lpNumberOfBytesRead: LongInt; nNumberOfBytesToRead: LongInt; 
                   lpBuffer: Pointer; hFile: Text); external 'ReadFile';

procedure CloseHandle(hObject: Text); external 'CloseHandle';

function GetLastError: LongInt; external 'GetLastError';

function GetTickCount: LongInt; external 'GetTickCount';

procedure ExitProcess(uExitCode: Integer); external 'ExitProcess';



// Initialization


procedure InitSystem;
begin
Heap := GetProcessHeap;

StdInputHandle := Text(GetStdHandle(STD_INPUT_HANDLE));
StdOutputHandle := Text(GetStdHandle(STD_OUTPUT_HANDLE));
SetConsoleMode($02F5, StdInputHandle);                      // set all flags except ENABLE_LINE_INPUT and ENABLE_WINDOW_INPUT 
IOError := 0;
end;



// Timer


function Timer: LongInt;
begin
Result := GetTickCount;
end;




// Heap operations


procedure GetMem(var P: Pointer; Size: Integer);
begin
P := HeapAlloc(Size, 0, Heap);
end;




procedure FreeMem(var P: Pointer; Size: Integer);
begin
HeapFree(P, 0, Heap);
end;




// Mathematical routines


procedure Randomize;
begin
RandSeed := Timer;
end;




function Random: Real;
begin
RandSeed := 1975433173 * RandSeed;
Result := 0.5 * (RandSeed / $7FFFFFFF + 1.0);
end;




function Min(x, y: Real): Real;
begin
if x < y then Result := x else Result := y;
end;




function IMin(x, y: Integer): Integer;
begin
if x < y then Result := x else Result := y;
end;





function Max(x, y: Real): Real;
begin
if x > y then Result := x else Result := y;
end;




function IMax(x, y: Integer): Integer;
begin
if x > y then Result := x else Result := y;
end;




// File and console I/O routines


procedure WriteConsole(Ch: Char);
var
  LenWritten: Integer;
begin
WriteFile(0, LenWritten, 1, @Ch, StdOutputHandle);
end;




procedure ReadConsole(var Ch: Char);
var
  LenRead: Integer;
begin
ReadFile(0, LenRead, 1, @Ch, StdInputHandle);
WriteConsole(Ch);
end;




procedure Rewrite(var F: Text; const Name: string);
begin
F := CreateFileA(0, FILE_ATTRIBUTE_NORMAL, CREATE_ALWAYS, nil, 0, GENERIC_WRITE, Name);
if Integer(F) = INVALID_HANDLE_VALUE then IOError := -2;
end;




procedure Reset(var F: Text; const Name: string);
begin
F := CreateFileA(0, FILE_ATTRIBUTE_NORMAL, OPEN_EXISTING, nil, 0, GENERIC_READ, Name);
if Integer(F) = INVALID_HANDLE_VALUE then IOError := -2;
end;




procedure Close(F: Text);
begin
CloseHandle(F);
end;



  
procedure BlockWrite(F: Text; Buf: Pointer; Len: Integer);
var
  LenWritten: Integer;
begin
WriteFile(0, LenWritten, Len, Buf, F);
end;




procedure BlockRead(F: Text; Buf: Pointer; Len: Integer; var LenRead: Integer);
begin
ReadFile(0, LenRead, Len, Buf, F);
if LenRead < Len then IOError := -1;
end;




procedure Seek(F: Text; Pos: Integer);
begin
Pos := SetFilePointer(FILE_BEGIN, nil, Pos, F);
end;




function FileSize(F: Text): Integer;
begin
Result := GetFileSize(nil, F);
end;




function FilePos(F: Text): Integer;
begin
Result := SetFilePointer(FILE_CURRENT, nil, 0, F);
end;




function EOF(F: Text): Boolean;
begin
if Integer(F) = 0 then
  Result := FALSE
else
  Result := FilePos(F) >= FileSize(F);
end;




function IOResult: Integer;
begin
Result := IOError;
IOError := 0;
end;




procedure WriteCh(F: Text; P: PStream; ch: Char);
var
  Dest: PChar;
begin
if P <> nil then             // String stream output
  begin                      
  Dest := PChar(Integer(P^.Data) + P^.Index);
  Dest^ := ch;
  Inc(P^.Index);
  end
else  
  if Integer(F) = 0 then     // Console output
    WriteConsole(ch)                
  else                       // File output
    BlockWrite(F, @ch, 1);   
end;




procedure WriteInt(F: Text; P: PStream; Number: Integer);
var
  Digit, Weight: Integer;
  Skip: Boolean;

begin
if Number = 0 then
  WriteCh(F, P,  '0')
else
  begin
  if Number < 0 then
    begin
    WriteCh(F, P,  '-');
    Number := -Number;
    end;

  Weight := 1000000000;
  Skip := TRUE;

  while Weight >= 1 do
    begin
    if Number >= Weight then Skip := FALSE;

    if not Skip then
      begin
      Digit := Number div Weight;
      WriteCh(F, P,  Char(ShortInt('0') + Digit));
      Number := Number - Weight * Digit;
      end;

    Weight := Weight div 10;
    end; // while
  end; // else

end;




procedure WriteHex(F: Text; P: PStream; Number: Integer; Digits: ShortInt);
var
  i, Digit: ShortInt;
begin
for i := Digits - 1 downto 0 do
  begin
  Digit := (Number shr (i shl 2)) and $0F;
  if Digit <= 9 then Digit := ShortInt('0') + Digit else Digit := ShortInt('A') + Digit - 10;
  WriteCh(F, P,  Char(Digit));
  end; 
end;





procedure WritePointer(F: Text; P: PStream; Number: Integer);
begin
WriteHex(F, P, Number, 8);
end;





procedure WriteReal(F: Text; P: PStream; Number: Real);
const
  FracBits = 16;
var
  Integ, Frac, InvWeight, Digit, IntegExpon: Integer;
  Expon: Real;

begin
// Write sign
if Number < 0 then
  begin
  WriteCh(F, P,  '-');
  Number := -Number;
  end;

// Normalize number
if Number = 0 then Expon := 0 else Expon := ln(Number) / ln(10);
if (Expon > 8) or (Expon < -3) then
  begin
  IntegExpon := Trunc(Expon);
  if IntegExpon < 0 then Dec(IntegExpon);
  Number := Number / exp(IntegExpon * ln(10));
  end
else
  IntegExpon := 0;  

// Write integer part
Integ := Trunc(Number);
Frac  := Round((Number - Integ) * (1 shl FracBits));

WriteInt(F, P, Integ);  WriteCh(F, P, '.');

// Write fractional part
InvWeight := 10;

while InvWeight <= 10000 do
  begin
  Digit := (Frac * InvWeight) shr FracBits;
  if Digit > 9 then Digit := 9;
  WriteCh(F, P,  Char(ShortInt('0') + Digit));
  Frac := Frac - (Digit shl FracBits) div InvWeight;
  InvWeight := InvWeight * 10;
  end; // while

// Write exponent
if IntegExpon <> 0 then 
  begin
  WriteCh(F, P, 'e');  WriteInt(F, P, IntegExpon);
  end;
 
end;




procedure WriteString(F: Text; P: PStream; const s: string);
var
  i: Integer;
begin
i := 0;
while s[i] <> #0 do
  begin
  WriteCh(F, P, s[i]);
  Inc(i);
  end; 
end;




procedure WriteBoolean(F: Text; P: PStream; Flag: Boolean);
begin
if Flag then WriteString(F, P, 'TRUE') else WriteString(F, P, 'FALSE');
end;




procedure WriteNewLine(F: Text; P: PStream);
begin
WriteCh(F, P, #13);  WriteCh(F, P, #10);
end;




procedure ReadCh(F: Text; P: PStream; var ch: Char);
var
  Len: Integer;
  Dest: PChar;
begin
if P <> nil then                // String stream input
  begin                      
  Dest := PChar(Integer(P^.Data) + P^.Index);
  ch := Dest^;
  Inc(P^.Index);
  end
else  
  if Integer(F) = 0 then        // Console input
    begin
    ReadConsole(ch);
    if ch = #13 then WriteConsole(#10);
    end 
  else                          // File input
    begin
    BlockRead(F, @ch, 1, Len);
    if ch = #10 then BlockRead(F, @ch, 1, Len);
    if Len <> 1 then ch := #0;
    end;

LastReadChar := ch;             // Required by ReadNewLine
end;




procedure ReadInt(F: Text; P: PStream; var Number: Integer);
var
  Ch: Char;
  Negative: Boolean;

begin
Number := 0;

// Read sign
Negative := FALSE;
ReadCh(F, P, Ch);
if Ch = '+' then
  ReadCh(F, P, Ch)
else if Ch = '-' then   
  begin
  Negative := TRUE;
  ReadCh(F, P, Ch);
  end;

// Read number
while (Ch >= '0') and (Ch <= '9') do
  begin
  Number := Number * 10 + ShortInt(Ch) - ShortInt('0');
  ReadCh(F, P, Ch);
  end; 

if Negative then Number := -Number;
end;




procedure ReadReal(F: Text; P: PStream; var Number: Real);
var
  Ch: Char;
  Negative, ExponNegative: Boolean;
  Weight: Real;
  Expon: Integer;
 
begin
Number := 0;
Expon := 0;

// Read sign
Negative := FALSE;
ReadCh(F, P, Ch);
if Ch = '+' then
  ReadCh(F, P, Ch)
else if Ch = '-' then   
  begin
  Negative := TRUE;
  ReadCh(F, P, Ch);
  end;

// Read integer part
while (Ch >= '0') and (Ch <= '9') do
  begin
  Number := Number * 10 + ShortInt(Ch) - ShortInt('0');
  ReadCh(F, P, Ch);
  end;

if Ch = '.' then                     // Fractional part found
  begin
  ReadCh(F, P, Ch);

  // Read fractional part
  Weight := 0.1;
  while (Ch >= '0') and (Ch <= '9') do
    begin
    Number := Number + Weight * (ShortInt(Ch) - ShortInt('0'));
    Weight := Weight / 10;
    ReadCh(F, P, Ch);
    end;
  end;

if (Ch = 'E') or (Ch = 'e') then     // Exponent found
  begin
  // Read exponent sign
  ExponNegative := FALSE;
  ReadCh(F, P, Ch);
  if Ch = '+' then
    ReadCh(F, P, Ch)
  else if Ch = '-' then   
    begin
    ExponNegative := TRUE;
    ReadCh(F, P, Ch);
    end;

  // Read exponent
  while (Ch >= '0') and (Ch <= '9') do
    begin
    Expon := Expon * 10 + ShortInt(Ch) - ShortInt('0');
    ReadCh(F, P, Ch);
    end;

  if ExponNegative then Expon := -Expon;
  end;
     
if Expon <> 0 then Number := Number * exp(Expon * ln(10));
if Negative then Number := -Number;
end;




procedure ReadString(F: Text; P: PStream; const s: string);
var
  i: Integer;
  Ch: Char;
begin
i := 0;
ReadCh(F, P, Ch);

while Ch <> #13 do
  begin
  s[i] := Ch;
  Inc(i);
  ReadCh(F, P, Ch);
  end;

s[i] := #0;
end;




procedure ReadNewLine(F: Text; P: PStream);
var
  Ch: Char;
begin
Ch := LastReadChar;
while not EOF(F) and (Ch <> #13) do ReadCh(F, P, Ch);
LastReadChar := #0;
end;




// String manipulation routines


function Length(const s: string): Integer;
begin
Result := 0;
while s[Result] <> #0 do Inc(Result);
end;





procedure StrCopy(var Dest: string; const Source: string);
var
  i: Integer;
begin
i := -1;
repeat
  Inc(i);
  Dest[i] := Source[i];
until Source[i] = #0;
end;





procedure StrCat(var Dest: string; const Source: string);
var
  i, j: Integer;
begin
i := 0;
while Dest[i] <> #0 do Inc(i);
j := -1;
repeat 
  Inc(j);
  Dest[i + j] := Source[j];
until Source[j] = #0;
end;





function StrComp(const s1, s2: string): Integer;
var
  i: Integer;
begin
Result := 0;
i := -1;
repeat 
  Inc(i);
  Result := Integer(s1[i]) - Integer(s2[i]);
until (s1[i] = #0) or (s2[i] = #0) or (Result <> 0);
end;





procedure CopyMemory(Dest, Source: Pointer; Len: LongInt);
var
  i: Integer;
  CurDest, CurSource: PChar;
begin
for i := 0 to Len - 1 do
  begin
  CurDest := PChar(Integer(Dest) + i);
  CurSource := PChar(Integer(Source) + i);
  CurDest^ := CurSource^;
  end;
end;





function ParseCmdLine(Index: Integer; var Str: string): Integer;
var
  CmdLine: string;
  CmdLinePtr: ^string;
  Param: string;
  ParamPtr: array [0..7] of ^string;
  i, NumParam, CmdLineLen: Integer;

begin
CmdLinePtr := GetCommandLineA;
CmdLine := CmdLinePtr^;
CmdLineLen := Length(CmdLine);

NumParam := 1;
ParamPtr[NumParam - 1] := @CmdLine;

for i := 0 to CmdLineLen - 1 do
  begin
  if CmdLine[i] <= ' ' then
    CmdLine[i] := #0;
    
  if (i > 0) and (CmdLine[i] > ' ') and (CmdLine[i - 1] = #0) then
    begin
    Inc(NumParam);
    ParamPtr[NumParam - 1] := @CmdLine[i];
    end;
  end;
  
if Index < NumParam then
  Str := ParamPtr[Index]^
else
  Str := '';

Result := NumParam;  
end;




// Conversion routines


procedure Val(const s: string; var Number: Real; var Code: Integer);
var
  Stream: TStream;
begin
Stream.Data := @s;
Stream.Index := 0;

ReadReal(Text(0), @Stream, Number);

if Stream.Index - 1 <> Length(s) then Code := Stream.Index - 1 else Code := 0;
end;





procedure Str(Number: Real; var s: string);
var
  Stream: TStream;
begin
Stream.Data := @s;
Stream.Index := 0;

WriteReal(Text(0), @Stream, Number);
s[Stream.Index] := #0;
end;





procedure IVal(const s: string; var Number: Integer; var Code: Integer);
var
  Stream: TStream;
begin
Stream.Data := @s;
Stream.Index := 0;

ReadInt(Text(0), @Stream, Number);

if Stream.Index - 1 <> Length(s) then Code := Stream.Index - 1 else Code := 0;
end;





procedure IStr(Number: Integer; var s: string);
var
  Stream: TStream;
begin
Stream.Data := @s;
Stream.Index := 0;

WriteInt(Text(0), @Stream, Number);
s[Stream.Index] := #0;
end;





function UpCase(ch: Char): Char;
begin
if (ch >= 'a') and (ch <= 'z') then
  Result := Chr(Ord(ch) - Ord('a') + Ord('A'))
else
  Result := ch;
end;  


