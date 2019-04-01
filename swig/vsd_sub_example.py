# Test python client to exercise DSTC.
import vsd

def signal_processor(signal, value):
    print("Got server call {}".format(arg))




# PythonBinaryOp class is defined and derived from C++ class BinaryOp


if __name__ == "__main__":

    ctx = vsd.create_context()
    vsd.set_callback(ctx, signal_processor)
    vsd.load_from_file(ctx, "../vss_rel_2.0.0-alpha+005.csv")
    sig = vsd.signal(ctx, "Vehicle.Drivetrain");
    vsd.subscribe(ctx, sig)
    vsd.process_events(-1)
