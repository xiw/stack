// TODO: %cc %s | sed 's/shl i32/shl nsw i32/' | antiopt -S -loop-prepare -loop-rotate -bugon-int -bugon-loop -anti-dce -simplifycfg | FileCheck %s
//
// http://blog.regehr.org/archives/767
// http://gcc.gnu.org/bugzilla/show_bug.cgi?id=54027

// CHECK:      define i32 @foo()
// CHECK-NEXT: entry:
// CHECK-NEXT:   unreachable
int foo()
{
  int x = 1;
  while (x) x <<= 1;
  return x;
}
