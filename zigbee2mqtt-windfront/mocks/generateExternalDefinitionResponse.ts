import type { ResponseMessage } from "../src/types.js";

export const GENERATE_EXTERNAL_DEFINITION_RESPONSE: ResponseMessage<"bridge/response/device/generate_external_definition"> = {
    payload: {
        status: "ok",
        data: {
            id: "$ID",
            source: `import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

export default {
    zigbeeModel: ['random model'],
    model: 'random model',
    vendor: 'random vendor',
    description: 'Automatically generated definition',
    extend: [m.temperature(), m.onOff({"powerOnBehavior":false})],
    meta: {},
};`,
        },
    },
    topic: "bridge/response/device/generate_external_definition",
};
