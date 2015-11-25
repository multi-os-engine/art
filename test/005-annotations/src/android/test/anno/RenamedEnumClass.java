package android.test.anno;

import java.lang.annotation.*;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)

public @interface RenamedEnumClass {
    enum RenamedEnum { FOO, BAR };
}
