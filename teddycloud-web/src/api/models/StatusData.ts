/* tslint:disable */
/* eslint-disable */
/**
 * TeddyCloud API
 * OpenAPI specification for TeddyCloud Backend API
 *
 * The version of the OpenAPI document: 1.0.1
 * 
 *
 * NOTE: This class is auto generated by OpenAPI Generator (https://openapi-generator.tech).
 * https://openapi-generator.tech
 * Do not edit the class manually.
 */

import { exists, mapValues } from '../runtime';
import type { StatusDataAttributes } from './StatusDataAttributes';
import {
    StatusDataAttributesFromJSON,
    StatusDataAttributesFromJSONTyped,
    StatusDataAttributesToJSON,
} from './StatusDataAttributes';

/**
 * 
 * @export
 * @interface StatusData
 */
export interface StatusData {
    /**
     * Object title: status
     * @type {string}
     * @memberof StatusData
     */
    type: string;
    /**
     * Application name
     * @type {string}
     * @memberof StatusData
     */
    id: string;
    /**
     * 
     * @type {StatusDataAttributes}
     * @memberof StatusData
     */
    attributes: StatusDataAttributes;
}

/**
 * Check if a given object implements the StatusData interface.
 */
export function instanceOfStatusData(value: object): boolean {
    let isInstance = true;
    isInstance = isInstance && "type" in value;
    isInstance = isInstance && "id" in value;
    isInstance = isInstance && "attributes" in value;

    return isInstance;
}

export function StatusDataFromJSON(json: any): StatusData {
    return StatusDataFromJSONTyped(json, false);
}

export function StatusDataFromJSONTyped(json: any, ignoreDiscriminator: boolean): StatusData {
    if ((json === undefined) || (json === null)) {
        return json;
    }
    return {
        
        'type': json['type'],
        'id': json['id'],
        'attributes': StatusDataAttributesFromJSON(json['attributes']),
    };
}

export function StatusDataToJSON(value?: StatusData | null): any {
    if (value === undefined) {
        return undefined;
    }
    if (value === null) {
        return null;
    }
    return {
        
        'type': value.type,
        'id': value.id,
        'attributes': StatusDataAttributesToJSON(value.attributes),
    };
}

