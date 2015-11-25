package android.test.anno;

public class RenamedNoted {
    @AnnoRenamedEnumMethod(renamed=RenamedEnumClass.RenamedEnum.BAR)
    public int bar() {
        return 0;
    }
}
