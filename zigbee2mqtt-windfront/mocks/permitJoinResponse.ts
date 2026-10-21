import type { ResponseMessage } from "../src/types.js";

export const PERMIT_JOIN_RESPONSE: ResponseMessage<"bridge/response/permit_join"> = {
    payload: {
        status: "ok",
        data: {
            time: 254,
        },
    },
    topic: "bridge/response/permit_join",
};
