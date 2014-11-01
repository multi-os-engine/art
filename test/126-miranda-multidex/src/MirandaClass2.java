class MirandaClass2 extends MirandaAbstract {
    public boolean inInterface() {
        return true;
    }

    public int inInterface2() {
        return 28;
    }

    // Better not hit any of these...
    public void inInterfaceDummy1() {
        System.out.println("inInterfaceDummy1");
    }
    public void inInterfaceDummy2() {
        System.out.println("inInterfaceDummy2");
    }
    public void inInterfaceDummy3() {
        System.out.println("inInterfaceDummy3");
    }
    public void inInterfaceDummy4() {
        System.out.println("inInterfaceDummy4");
    }
    public void inInterfaceDummy5() {
        System.out.println("inInterfaceDummy5");
    }
}
