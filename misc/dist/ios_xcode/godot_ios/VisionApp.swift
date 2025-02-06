
#if TARGET_OS_VISION
import SwiftUI
import CompositorServices

struct MetalLayerConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities,
                           configuration: inout LayerRenderer.Configuration)
    {
        let supportsFoveation = capabilities.supportsFoveation
        let supportedLayouts = capabilities.supportedLayouts(options: supportsFoveation ? [.foveationEnabled] : [])
        
        // The device supports the `dedicated` and `layered` layouts, and optionally `shared` when foveation is disabled
        // The simulator supports the `dedicated` and `shared` layouts.
        // However, since we use vertex amplification to implement shared rendering, it won't work on the simulator in this project.
        configuration.layout = supportedLayouts.contains(.layered) ? .layered : .dedicated
        print("layout: \(configuration.layout)")
        configuration.isFoveationEnabled = supportsFoveation
        print("supportsFoveation: \(supportsFoveation)")
        configuration.colorFormat = .rgba16Float
    }
}


@main
struct FullyImmersiveMetalApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @State var immersionStyle: (any ImmersionStyle) = FullImmersionStyle.full

    var body: some Scene {
        WindowGroup {
            ContentView($immersionStyle)
                .frame(minWidth: 480, maxWidth: 480, minHeight: 200, maxHeight: 320)
        }
        .windowResizability(.contentSize)

        ImmersiveSpace(id: "ImmersiveSpace") {
            CompositorLayer(configuration: MetalLayerConfiguration()) { layerRenderer in
                AppDelegate.viewController.setup(layerRenderer)            }
        }
        .immersionStyle(selection: $immersionStyle, in: .mixed, .full)
    }
}

struct ContentView: View {
    @Environment(\.openImmersiveSpace) var openImmersiveSpace
    @Environment(\.dismissImmersiveSpace) var dismissImmersiveSpace
    @Environment(\.dismiss) private var dismiss

    @Binding private var immersionStyle: any ImmersionStyle

    @State private var showImmersiveSpace = false
    @State private var useMixedImmersion = false
    @State private var passthroughCutoffAngle = 60.0
    @State var isLoading: Bool = true


    init(_ immersionStyle: Binding<any ImmersionStyle>) {
        _immersionStyle = immersionStyle
    }

    var body: some View {
        VStack {
            if(isLoading){
                Image("SplashImage")
            }
        }
        .task {
            AppDelegate.viewController.swiftController = SwiftInterop(showImmersiveSpace: $showImmersiveSpace, useMixedImmersion: $useMixedImmersion, isLoading: $isLoading)
            AppDelegate.viewController.viewDidAppear();
        }
        .onChange(of: showImmersiveSpace) { _, newValue in
            Task {
                if newValue {
                    await openImmersiveSpace(id: "ImmersiveSpace")
                } else {
                    await dismissImmersiveSpace()
                }
            }
        }
        .onChange(of: useMixedImmersion) { _, _ in
            immersionStyle = useMixedImmersion ? .mixed : .full
        }
        .onChange(of: passthroughCutoffAngle) { oldValue, newValue in
//            rendererConfiguration.portalCutoffAngle = newValue
        }
        .onChange(of:isLoading) { old,newValue in
            if(!newValue){
                dismiss()
                
            }
        }
            
    }
}


@objcMembers
public class SwiftInterop: NSObject, SwiftVisionController {

    
    @Binding var showImmersiveSpace: Bool
    @Binding var useMixedImmersion: Bool
    @Binding var isLoading: Bool
    
    init(showImmersiveSpace: Binding<Bool>, useMixedImmersion: Binding<Bool>, isLoading: Binding<Bool>) {
        _showImmersiveSpace = showImmersiveSpace
        _useMixedImmersion = useMixedImmersion
        _isLoading = isLoading
    }
    
  
    public func finishedLoading() {
        isLoading = false;
    }
    public func setImmersiveSpace(_ immersive: Bool) {
        showImmersiveSpace = immersive
    }
    
    public func present(_ viewControllerToPresent: UIViewController!) {
        //TODO: Present the view controller
    }
    
}
#endif
