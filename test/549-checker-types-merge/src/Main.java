/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

interface InterfaceSuper {}
interface InterfaceOtherSuper {}

interface InterfaceAExtendsSuper extends InterfaceSuper {}
interface InterfaceBExtendsSuper extends InterfaceSuper {}
interface InterfaceCExtendsA extends InterfaceAExtendsSuper {}
interface InterfaceDExtendsB extends InterfaceBExtendsSuper {}

class ClassSuper {}
class ClassOtherSuper {}

class ClassAExtendsSuper extends ClassSuper {}
class ClassBExtendsSuper extends ClassSuper {}
class ClassCExtendsA extends ClassAExtendsSuper {}
class ClassDExtendsB extends ClassBExtendsSuper {}

class ClassAImplementsInterfaceAExtendsSuper implements InterfaceSuper {}

public class Main {

  /// CHECK-START: java.lang.Object Main.testMergeNullContant(boolean) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:Main
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeNullContant(boolean cond) {
    return cond ? null : new Main();
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassCExtendsA, ClassDExtendsB) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassCExtendsA a, ClassDExtendsB b) {
    // Different classes, have a common super type.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassCExtendsA, ClassSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassCExtendsA a, ClassSuper b) {
    // Different classes, one is the super type of the other.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassSuper, ClassSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:ClassSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassSuper a, ClassSuper b) {
    // Same classes.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClasses(boolean, ClassOtherSuper, ClassSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClasses(boolean cond, ClassOtherSuper a, ClassSuper b) {
    // Different classes, have Object as the common super type.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClassWithInterface(boolean, ClassAImplementsInterfaceAExtendsSuper, InterfaceSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClassWithInterface(boolean cond, ClassAImplementsInterfaceAExtendsSuper a, InterfaceSuper b) {
    // Class implements interface.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeClassWithInterface(boolean, ClassSuper, InterfaceSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeClassWithInterface(boolean cond, ClassSuper a, InterfaceSuper b) {
    // Class doesn't implement interface.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceCExtendsA, InterfaceSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceCExtendsA a, InterfaceSuper b) {
    // Different Interfaces, one implements the other.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceSuper, InterfaceSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceSuper a, InterfaceSuper b) {
    // Same interfaces.
    return cond ? a : b;
  }

  /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceCExtendsA, InterfaceDExtendsB) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:InterfaceSuper
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceCExtendsA a, InterfaceDExtendsB b) {
    // Different Interfaces, have a common super type.
    return cond ? a : b;
  }

    /// CHECK-START: java.lang.Object Main.testMergeInterfaces(boolean, InterfaceSuper, InterfaceOtherSuper) reference_type_propagation (after)
  /// CHECK:      <<Phi:l\d+>>       Phi klass:java.lang.Object
  /// CHECK:                         Return [<<Phi>>]
  private Object testMergeInterfaces(boolean cond, InterfaceSuper a, InterfaceOtherSuper b) {
    // Different interfaces.
    return cond ? a : b;
  }

  public static void main(String[] args) {
  }
}
