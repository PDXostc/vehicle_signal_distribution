
# Test python client to exercise DSTC.
import vsd



if __name__ == "__main__":
    ctx = vsd.create_context()
    vsd.load_from_file(ctx, "../vss_rel_2.0.0-alpha+005.csv")


    vsd.process_events(300000)

    sig = vsd.signal(ctx, "Vehicle.Drivetrain.Transmission.Gear")
    vsd.set(ctx, sig, 4)

    pub = vsd.signal(ctx, "Vehicle.Drivetrain.Transmission")
    vsd.publish(pub);
    print("Should be 4: {}".format(vsd.get(ctx, sig)))
    vsd.process_events(300000)
