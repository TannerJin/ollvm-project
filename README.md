### Functions

* -bcf

  > 虚假控制流 (优化过)

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

  > 反FishHook & C函数调用混淆
  
* -och(私有)
  > Objective-C Call Hidden (OC 函数调用混淆)


### Features   

1. support `libclang_rt.ios.a`   

3. support `Enable Index-While-Building Functionality`

2. support `__attribute__((__annotate__(("bcf; fla"; "sub"; "afh"; "och"))))` for ObjCMethod

