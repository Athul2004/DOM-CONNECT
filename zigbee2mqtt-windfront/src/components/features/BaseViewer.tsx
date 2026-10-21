import { memo } from "react";
import type { BasicFeature } from "../../types.js";
import DisplayValue from "../value-decorators/DisplayValue.js";
import type { BaseFeatureProps } from "./index.js";

const BaseViewer = memo(({ feature, deviceValue }: BaseFeatureProps<BasicFeature>) => {
    return (
        <div>
            {feature.property && (
                <span className="font-bold">
                    <DisplayValue value={deviceValue} name={feature.name} />
                </span>
            )}
            {"unit" in feature && <span className="text-xs ms-1">{feature.unit}</span>}
        </div>
    );
});

export default BaseViewer;
