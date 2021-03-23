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
   
* Obfuscate Objc-Class Name
  ```objc
  __attribute__((objc_runtime_name("Obfuscate Name")))
  @interface JailbreakCheck : NSObjec
  
  @end
  ```

* -afh(私有)

  > Anti-FishHook (非`dlsym`方式)
  
* -och(私有)
  > Objective-C Call Hidden


### Features   

1. support `libclang_rt.ios.a`   

3. support `Enable Index-While-Building Functionality`

2. support `__attribute__((__annotate__(("bcf; fla"; "sub"; "afh"; "och"))))` for ObjCMethod

