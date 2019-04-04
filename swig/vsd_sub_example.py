# Test python client to exercise DSTC.
import vsd
ctx = None
def process_signal(signal, path, value):
    print("YES {}:{} - {}".format(signal, path, value))

if __name__ == "__main__":
    ctx = vsd.create_context()
    vsd.set_callback(ctx, process_signal)
    vsd.load_from_file(ctx, "../vss_rel_2.0.0-alpha+005.csv")
    sig = vsd.signal(ctx, "Vehicle.Drivetrain");
    vsd.subscribe(ctx, sig)
    vsd.process_events(-1)
