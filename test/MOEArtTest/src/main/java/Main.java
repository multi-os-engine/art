import ios.NSObject;
import ios.foundation.NSDictionary;
import ios.uikit.*;
import ios.uikit.c.UIKit;
import ios.uikit.enums.NSLayoutAttribute;
import ios.uikit.enums.NSLayoutRelation;
import ios.uikit.protocol.UIApplicationDelegate;

import org.moe.natj.general.Pointer;
import org.moe.natj.general.ann.Generated;

public class Main extends NSObject implements UIApplicationDelegate {

    public static void main(String[] args) {
        UIKit.UIApplicationMain(0, null, null, Main.class.getName());
    }

    @Generated("NatJ")
    public static native Main alloc();

    @Generated("NatJ")
    protected Main(Pointer peer) {
        super(peer);
    }

    private UIWindow window;

    @Override
    public boolean applicationDidFinishLaunchingWithOptions(
            UIApplication application, NSDictionary launchOptions) {
        window = UIWindow.alloc().initWithFrame(UIScreen.mainScreen().bounds());

        UIViewController controller = UIViewController.alloc().init();
        window.setRootViewController(controller);

        window.makeKeyAndVisible();

        UILabel label = UILabel.alloc().init();
        label.setTranslatesAutoresizingMaskIntoConstraints(false);
        label.setText("Hello MOE!");
        label.setFont(UIFont.boldSystemFontOfSize(34.0));
        controller.view().addSubview(label);

        NSLayoutConstraint constraint = NSLayoutConstraint.constraintWithItemAttributeRelatedByToItemAttributeMultiplierConstant(label,
                NSLayoutAttribute.CenterX, NSLayoutRelation.Equal, controller.view(), NSLayoutAttribute.CenterX, 1.0, 0.0);
        controller.view().addConstraint(constraint);
        constraint = NSLayoutConstraint.constraintWithItemAttributeRelatedByToItemAttributeMultiplierConstant(label,
                NSLayoutAttribute.CenterY, NSLayoutRelation.Equal, controller.view(), NSLayoutAttribute.CenterY, 1.0, 0.0);
        controller.view().addConstraint(constraint);

        return true;
    }

    @Override
    public void setWindow(UIWindow value) {
        window = value;
    }

    @Override
    public UIWindow window() {
        return window;
    }
}
