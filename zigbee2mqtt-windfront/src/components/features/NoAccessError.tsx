import { memo } from "react";
import type { BasicFeature, FeatureWithSubFeatures } from "../../types.js";
import type { BaseFeatureProps } from "./index.js";

const NoAccessError = memo((props: BaseFeatureProps<BasicFeature | FeatureWithSubFeatures>) => {
    return (
        <div className="alert alert-warning" role="alert">
            Unknown access <pre>{props.feature.access}</pre>
        </div>
    );
});

export default NoAccessError;
