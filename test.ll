define i32 @main() {
entry:
  %sum = add i32 3, 5
  %othersum = add i32 %sum, 1
  ret i32 %othersum
}