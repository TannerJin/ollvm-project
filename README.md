### Functions

* -bcf

  > 虚假控制流

* -fla

  > 平坦化

* -sub

  > 指令替换
  
* Anti-ObjcHook

   ```objc
   @interface JailbreakCheck : NSObject

   + (Bool)isJailbreak __attribute__((objc_direct));

   @end
   ```

* -afh(私有)

  > Anti-FishHook
  
* -och(私有)
  > Objective-C调用反IDA


### Features   

1. support `libclang_rt.ios.a`   

3. support `Enable Index-While-Building Functionality`

2. support `__attribute__((__annotate__(("bcf; fla"; "sub"; "afh"; "och"))))` for ObjCMethod

