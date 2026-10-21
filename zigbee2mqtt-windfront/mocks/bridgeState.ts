import type { Zigbee2MQTTAPI } from "zigbee2mqtt";
import type { Message } from "../src/types.js";

export const BRIDGE_STATE: Message<Zigbee2MQTTAPI["bridge/state"]> = {
    payload: {
        state: "online",
    },
    topic: "bridge/state",
};
