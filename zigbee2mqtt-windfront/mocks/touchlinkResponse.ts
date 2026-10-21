import type { ResponseMessage } from "../src/types.js";

export const TOUCHLINK_RESPONSE: ResponseMessage<"bridge/response/touchlink/scan"> = {
    payload: {
        status: "ok",
        data: {
            found: [
                {
                    ieee_address: "xxxxx",
                    channel: 1,
                },
                {
                    ieee_address: "0x00124b001e73227f",
                    channel: 10,
                },
            ],
        },
    },
    topic: "bridge/response/touchlink/scan",
};
