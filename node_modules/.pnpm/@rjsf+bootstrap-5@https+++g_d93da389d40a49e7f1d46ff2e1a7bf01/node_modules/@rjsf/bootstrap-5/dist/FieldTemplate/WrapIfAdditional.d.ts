import React from "react";
import { JSONSchema7 } from "json-schema";
declare type WrapIfAdditionalProps = {
    children: React.ReactElement;
    classNames: string;
    disabled: boolean;
    id: string;
    label: string;
    onDropPropertyClick: (index: string) => (event?: any) => void;
    onKeyChange: (index: string) => (event?: any) => void;
    readonly: boolean;
    required: boolean;
    schema: JSONSchema7;
};
declare const WrapIfAdditional: ({ children, disabled, id, label, onDropPropertyClick, onKeyChange, readonly, required, schema, }: WrapIfAdditionalProps) => JSX.Element;
export default WrapIfAdditional;
