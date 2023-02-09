# peanut
Basic JavaScript to Squirrel transpiler, focusing on compatibility with Portal 2.

Currently implemented:
  - "let" and "var" keywords
  - simulted function closure
  - template literals
  - defining objects

Known issues:
  - "var" definitions cannot span multiple lines
  - locals in for loops are cloned to functions outside the loop
  - ternary operations and switch statements are broken
