package android.test.anno;

import java.lang.annotation.*;

@Retention(RetentionPolicy.RUNTIME)
public @interface AnnotationWithMissingClassMember {
    Class value();
}
