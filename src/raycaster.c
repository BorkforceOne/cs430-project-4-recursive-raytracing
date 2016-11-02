//
// Created by Brandon Garling on 9/21/2016.
//

#include <stdlib.h>
#include <stdio.h>
#include "3dmath.h"
#include "raycaster.h"
#include "imaging.h"

#define MAX_RECURSE_DEPTH 6

/**
 * Allocates space in the imageRef specified for an image of the selected imageWidth and imageHeight.
 * Then raycasts a specified scene into the specified image.
 * @param sceneRef - The input scene to render
 * @param imageRef - The output image to write to
 * @param imageWidth - The height of the output image
 * @param imageHeight - The width of the output image
 * @return 0 if success, otherwise a failure occurred
 */
int raycast(Scene *sceneRef, Image* imageRef, int imageWidth, int imageHeight) {

	imageRef->width = (uint32_t) imageWidth;
	imageRef->height= (uint32_t) imageHeight;
	imageRef->pixmapRef = malloc(sizeof(RGBApixel) * imageWidth * imageHeight);

	double cameraHeight = sceneRef->camera.height;
	double cameraWidth = sceneRef->camera.width;

	V3 viewPlanePos = {0, 0, 1};
    V3 cameraPos = {0, 0, 0};

	double pixelHeight = cameraHeight/imageWidth;
	double pixelWidth = cameraWidth/imageHeight;

	V3 rayDirection = {0, 0, 0}; // The direction of our ray
    V3 point = {0, 0, 0}; // The point on the viewPlane that we intersect

	RGBAColor colorFound;

    point.data.Z = viewPlanePos.data.Z;
	for (int i=0; i<imageWidth; i++) {
        point.data.Y = -(viewPlanePos.data.Y - cameraHeight/2.0 + pixelHeight * (i + 0.5));
		for (int j=0; j<imageHeight; j++) {
            point.data.X = viewPlanePos.data.X - cameraWidth/2.0 + pixelWidth * (j + 0.5);
			v3_normalize(&point, &rayDirection); // normalization, find the ray direction
			shoot(&cameraPos, &rayDirection, sceneRef, &colorFound);
			shade(&colorFound, &imageRef->pixmapRef[i*imageHeight + j]);
		}
	}


	return 0;
}

/**
 * Shades a specific pixel based on the primitive provides
 * @param primitiveHitRef - The primitive that was hit during a call to the shoot function
 * @param pixel  - The pixel to color
 * @return 0 if success, otherwise a failure occurred
 */
int shade(RGBAColor* colorRef, RGBApixel *pixel) {
	pixel->r = colorRef->data.R;
	pixel->g = colorRef->data.G;
	pixel->b = colorRef->data.B;
	pixel->a = 255;
	return 0;
}

/**
 * Does the actual raytracing and sets the primitiveHit to a pointer to the primitive that was hit,
 * if any, when shooting the ray.
 * @param rayOriginRef - The origin of the ray
 * @param rayDirectionRef - The direction of the ray
 * @param sceneRef - A reference to the current scene
 * @param primitiveHit - A reference to a refrence of a primitive to set to the hit reference if a hit occurs
 * @return
 */
int shoot(V3 *rayOriginRef, V3 *rayDirectionRef, Scene *sceneRef, RGBAColor *foundColor) {
	V3 color;
	if (shoot_rec(rayOriginRef, rayDirectionRef, sceneRef, &color, 0) != 0) {
		return 1;
	}

	// Figure out lighting
	foundColor->data.R = (uint8_t) (clamp(color.array[0])*255);
	foundColor->data.G = (uint8_t) (clamp(color.array[1])*255);
	foundColor->data.B = (uint8_t) (clamp(color.array[2])*255);
	foundColor->data.A = 1;

	return 0;
}

/**
 * Does the actual raytracing and sets the primitiveHit to a pointer to the primitive that was hit,
 * if any, when shooting the ray.
 * @param rayOriginRef - The origin of the ray
 * @param rayDirectionRef - The direction of the ray
 * @param sceneRef - A reference to the current scene
 * @param primitiveHit - A reference to a refrence of a primitive to set to the hit reference if a hit occurs
 * @return
 */
int shoot_rec(V3 *rayOriginRef, V3 *rayDirectionRef, Scene *sceneRef, V3 *foundColor, int depth) {
	Primitive *primitiveRef;
	Primitive *primitiveHitRef = NULL;

	foundColor->array[0] = 0;
	foundColor->array[1] = 0;
	foundColor->array[2] = 0;

	// Our current closest t value
	double primitive_t = INFINITY;
	// A possible t value replacement
	double possible_t;

	for (int i = 0; i < sceneRef->primitivesLength; i++) {
		possible_t = INFINITY;
		primitiveRef = sceneRef->primitives[i];

		switch(primitiveRef->type) {
			case PLANE_T:
				possible_t = intersect_plane(&primitiveRef->data.plane, rayOriginRef, rayDirectionRef);
				break;
			case SPHERE_T:
				possible_t = intersect_sphere(&primitiveRef->data.sphere, rayOriginRef, rayDirectionRef);
				break;
		}
		// Set the new possible shadow
		if (possible_t > 0 && possible_t < primitive_t) {
			primitive_t = possible_t;
			primitiveHitRef = primitiveRef;
		}
	}

	if (primitiveHitRef != NULL) {
		// ambient light
		V3 color = {0.1, 0.1, 0.1};

		V3 newRayOrigin;
		// Calculate our newRayOrigin
		v3_scale(rayDirectionRef, primitive_t, &newRayOrigin);
		v3_add(rayOriginRef, &newRayOrigin, &newRayOrigin);


		V3 hitToLightRayDirection;
		V3 rayReflectionDirection;
		double reflectivity;
		double refractivity;
		double ior;
		V3 normal;
		V3 colorDiffuse;
		V3 colorSpecular;
		V3 lightColor;
		V3 lightContribution;
		V3 diffuse;
		V3 specular;
		double frad;
		double fang;

		switch(primitiveHitRef->type) {
			case PLANE_T:
				reflectivity = primitiveHitRef->data.plane.reflectivity;
				refractivity = primitiveHitRef->data.plane.refractivity;
				ior = primitiveHitRef->data.plane.ior;
				normal = primitiveHitRef->data.plane.normal;
				colorDiffuse = primitiveHitRef->data.plane.diffuseColor;
				colorSpecular = primitiveHitRef->data.plane.specularColor;

				break;
			case SPHERE_T:
				reflectivity = primitiveHitRef->data.sphere.reflectivity;
				refractivity = primitiveHitRef->data.sphere.refractivity;
				ior = primitiveHitRef->data.sphere.ior;

				// Calculate normal
				v3_subtract(&newRayOrigin, &primitiveHitRef->data.sphere.position, &normal);
				v3_normalize(&normal, &normal);

				colorDiffuse = primitiveHitRef->data.sphere.diffuseColor;
				colorSpecular = primitiveHitRef->data.sphere.specularColor;
				break;
		}

		// Shadow test
		for (int i = 0; i < sceneRef->lightsLength; i++) {
			Light *lightRef;
			lightRef = sceneRef->lights[i];
			double light_t = INFINITY;
			double lightDistance = INFINITY;
			V3 lightPosition;

			// Figure out hitToLightRayDirection
			switch (lightRef->type) {
				case POINTLIGHT_T:
					lightPosition = lightRef->data.pointLight.position;
					lightColor = lightRef->data.pointLight.color;
					v3_subtract(&lightPosition, &newRayOrigin, &hitToLightRayDirection);
					v3_normalize(&hitToLightRayDirection, &hitToLightRayDirection);
					v3_distance(&lightPosition, &newRayOrigin, &lightDistance);
					break;
				case SPOTLIGHT_T:
					lightPosition = lightRef->data.spotLight.position;
					lightColor = lightRef->data.spotLight.color;
					v3_subtract(&lightPosition, &newRayOrigin, &hitToLightRayDirection);
					v3_normalize(&hitToLightRayDirection, &hitToLightRayDirection);
					v3_distance(&lightPosition, &newRayOrigin, &lightDistance);
					break;
			}

			// See if this should be in shadow
			for (int j = 0; j < sceneRef->primitivesLength; j++) {
				possible_t = INFINITY;
				primitiveRef = sceneRef->primitives[j];

				// Skip the current object
				if (primitiveRef == primitiveHitRef)
					continue;

				switch(primitiveRef->type) {
					case PLANE_T:
						possible_t = intersect_plane(&primitiveRef->data.plane, &newRayOrigin, &hitToLightRayDirection);
						break;
					case SPHERE_T:
						possible_t = intersect_sphere(&primitiveRef->data.sphere, &newRayOrigin, &hitToLightRayDirection);
						break;
				}
				// Set the new possible shadow
				if (possible_t > 0 && possible_t < lightDistance) {
					light_t = possible_t;
					break;
				}
			}

			if (light_t != INFINITY)
				// Our light is in shadow
				continue;

			// Calculate rayReflectionDirection
			v3_reflect(&hitToLightRayDirection, &normal, &rayReflectionDirection);

			// Get diffuse color contribution
			calculate_diffuse(&normal, &hitToLightRayDirection, &colorDiffuse, &lightColor, &diffuse);
			// Get specular color contribution
			calculate_specular(rayDirectionRef, &rayReflectionDirection, &colorSpecular, &lightColor, &normal, &hitToLightRayDirection, &specular);
			
			calculate_frad(lightRef, lightDistance, &frad);
			calculate_fang(lightRef, &hitToLightRayDirection, &fang);
			v3_add(&diffuse, &specular, &lightContribution);
			v3_scale(&lightContribution, frad * fang, &lightContribution);
			color.array[0] += lightContribution.array[0];
			color.array[1] += lightContribution.array[1];
			color.array[2] += lightContribution.array[2];
		}

		v3_add(&color, foundColor, foundColor);

		if (depth < MAX_RECURSE_DEPTH) {
			V3 reflectionColor;

			if (reflectivity > 0) {
				v3_reflect(rayDirectionRef, &normal, &rayReflectionDirection);

				shoot_rec(&newRayOrigin, &rayReflectionDirection, sceneRef, &reflectionColor, depth + 1);

				v3_scale(&reflectionColor, reflectivity, &reflectionColor);

				v3_add(&reflectionColor, foundColor, foundColor);
			}
		}
	}

	return 0;
}


/**
 * Clamp a value between 0 and 1
 * @param a
 * @return
 */
double clamp(double a) {
	if (a < 0)
		return 0;
	if (a > 1)
		return 1;
	return a;
}

/**
 * Calculate the radial attenuation
 * @param light - The light to calculate for
 * @param distance - The distance from the light
 * @param result - The resulting frad calculation
 */
void calculate_frad(Light *light, double distance, double *result) {
	if (distance == INFINITY) {
		*result = 1;
		return;
	}

	*result = 1/(light->data.pointLight.radialA2*pow(distance, 2) +
			  light->data.pointLight.radialA1*distance +
			  light->data.pointLight.radialA0);
}

/**
 * Calculate the angular attenuation
 * @param light - The light to calculate for
 * @param V0 - The vector between the light and the object
 * @param result - The resulting fang calculation
 */
void calculate_fang(Light *light, V3 *V0, double *result) {
	if (light->type != SPOTLIGHT_T) {
		*result = 1;
		return;
	}
	V3 VLight;
    v3_scale(V0, -1, &VLight);

	double s;
	v3_dot(&light->data.spotLight.direction, &VLight, &s);

    if (s < cos(light->data.spotLight.theta)) {
        *result = 0;
    }
	else {
		*result = pow(s, light->data.spotLight.angularA0);
	}
}

/**
 * Calculate the diffuse color contribution
 * @param N - The normal of the primitive hit
 * @param L - The vector between the light and the object
 * @param K - The diffuse color of the object
 * @param I - The light's color
 * @param result - The resulting light diffuse contribution
 */
void calculate_diffuse(V3 *N, V3 *L, V3 *K, V3* I, V3* result) {
	double s;
	v3_dot(N, L, &s);
	if (s > 0) {
		v3_scale(I, s, result);
		result->array[0] *= K->array[0];
		result->array[1] *= K->array[1];
		result->array[2] *= K->array[2];
	}
	else {
		result->array[0] = 0;
		result->array[1] = 0;
		result->array[2] = 0;
	}
}

/**
 * Calculate the specular-color contribution
 * @param V - The view direction vector
 * @param R - The reflection vector
 * @param K - The specular color of the object
 * @param I - The light's color
 * @param N - The normal of the primitive hit
 * @param L - The vector between the light and the object
 * @param result - The resulting light specular contriubtion
 */
void calculate_specular(V3 *V, V3 *R, V3 *K, V3* I, V3* N, V3* L, V3* result) {
	double s1, s2;
	v3_dot(V, R, &s1);
	v3_dot(N, L, &s2);
	if (s1 > 0 && s2 > 0){
		s1 = pow(s1, 20);
		v3_scale(I, s1, result);
		result->array[0] *= K->array[0];
		result->array[1] *= K->array[1];
		result->array[2] *= K->array[2];
	}
	else {
		result->array[0] = 0;
		result->array[1] = 0;
		result->array[2] = 0;
	}
}

/**
 * Sphere intersection test
 * @param sphereRef - The sphere to check
 * @param rayOriginRef - The ray origin
 * @param rayDirectionRef - The ray direction
 * @return The hit distance between the rayOrigin and the sphere along the rayDirection, if positive. Otherwise INFINITY.
 */
double intersect_sphere(Sphere *sphereRef, V3 *rayOriginRef, V3 *rayDirectionRef) {
	double B = 2 * (rayDirectionRef->data.X * (rayOriginRef->data.X - sphereRef->position.data.X) + rayDirectionRef->data.Y*(rayOriginRef->data.Y - sphereRef->position.data.Y) + rayDirectionRef->data.Z*(rayOriginRef->data.Z - sphereRef->position.data.Z));
	double C = pow(rayOriginRef->data.X - sphereRef->position.data.X, 2) + pow(rayOriginRef->data.Y - sphereRef->position.data.Y, 2) + pow(rayOriginRef->data.Z - sphereRef->position.data.Z, 2) - pow(sphereRef->radius, 2);

	double discriminant = (pow(B, 2) - 4*C);
	if (discriminant < 0) {
		// No intersection
		return INFINITY;
	}

	double t_possible = (-B + sqrt(discriminant))/2;
	double t_possible2 = (-B - sqrt(discriminant))/2;
	if (t_possible || t_possible2 > 0) {
		if (t_possible > t_possible2)
			return t_possible2;
		else
			return t_possible;
	}

	return INFINITY;
}

/**
 *
 * @param planeRef - The plane to check
 * @param rayOriginRef - The ray origin
 * @param rayDirectionRef  - The ray direction
 * @return The hit distance between the rayOrigin and the plane along the rayDirection, if positive. Otherwise INFINITY.
 */
double intersect_plane(Plane *planeRef, V3 *rayOriginRef, V3 *rayDirectionRef) {
	double Vd;
	double V0;
	V3 vectorTemp;
	v3_subtract(rayOriginRef, &planeRef->position, &vectorTemp);
	v3_dot(&planeRef->normal, &vectorTemp, &Vd);

	if (Vd == 0) {
		// No intersection!
		return INFINITY;
	}

	v3_dot(&planeRef->normal, rayDirectionRef, &V0);

	double t_possible = -(Vd / V0);
	if (t_possible > 0)
		return t_possible;

	return INFINITY;
}