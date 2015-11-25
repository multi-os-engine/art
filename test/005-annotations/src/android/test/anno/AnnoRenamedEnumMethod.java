package android.test.anno;

import java.lang.annotation.*;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)

public @interface AnnoRenamedEnumMethod {
    RenamedEnumClass.RenamedEnum renamed() default RenamedEnumClass.RenamedEnum.FOO;
}


