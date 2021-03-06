/*******************************************************************************
 *
 * Module Name: utmisc - common utility procedures
 *              $Revision: 78 $
 *
 ******************************************************************************/

/*
 *  Copyright (C) 2000 - 2002, R. Byron Moore
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "acpi.h"
#include "acnamesp.h"
#include "amlcode.h"
#include "acinterp.h"


#define _COMPONENT          ACPI_UTILITIES
	 ACPI_MODULE_NAME    ("utmisc")


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_dword_byte_swap
 *
 * PARAMETERS:  Value           - Value to be converted
 *
 * DESCRIPTION: Convert a 32-bit value to big-endian (swap the bytes)
 *
 ******************************************************************************/

u32
acpi_ut_dword_byte_swap (
	u32                     value)
{
	union {
		u32                 value;
		u8                  bytes[4];
	} out;

	union {
		u32                 value;
		u8                  bytes[4];
	} in;


	ACPI_FUNCTION_ENTRY ();


	in.value = value;

	out.bytes[0] = in.bytes[3];
	out.bytes[1] = in.bytes[2];
	out.bytes[2] = in.bytes[1];
	out.bytes[3] = in.bytes[0];

	return (out.value);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_set_integer_width
 *
 * PARAMETERS:  Revision            From DSDT header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the global integer bit width based upon the revision
 *              of the DSDT.  For Revision 1 and 0, Integers are 32 bits.
 *              For Revision 2 and above, Integers are 64 bits.  Yes, this
 *              makes a difference.
 *
 ******************************************************************************/

void
acpi_ut_set_integer_width (
	u8                      revision)
{

	if (revision <= 1) {
		acpi_gbl_integer_bit_width = 32;
		acpi_gbl_integer_byte_width = 4;
	}
	else {
		acpi_gbl_integer_bit_width = 64;
		acpi_gbl_integer_byte_width = 8;
	}
}


#ifdef ACPI_DEBUG
/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_display_init_pathname
 *
 * PARAMETERS:  Obj_handle          - Handle whose pathname will be displayed
 *              Path                - Additional path string to be appended
 *
 * RETURN:      acpi_status
 *
 * DESCRIPTION: Display full pathnbame of an object, DEBUG ONLY
 *
 ******************************************************************************/

void
acpi_ut_display_init_pathname (
	acpi_handle             obj_handle,
	char                    *path)
{
	acpi_status             status;
	acpi_buffer             buffer;


	ACPI_FUNCTION_NAME ("Ut_display_init_pathname");


	buffer.length = ACPI_ALLOCATE_LOCAL_BUFFER;

	status = acpi_ns_handle_to_pathname (obj_handle, &buffer);
	if (ACPI_SUCCESS (status)) {
		if (path) {
			ACPI_DEBUG_PRINT ((ACPI_DB_INIT, "%s.%s\n", (char *) buffer.pointer, path));
		}
		else {
			ACPI_DEBUG_PRINT ((ACPI_DB_INIT, "%s\n", (char *) buffer.pointer));
		}

		ACPI_MEM_FREE (buffer.pointer);
	}
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_valid_acpi_name
 *
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
 *
 * DESCRIPTION: Check for a valid ACPI name.  Each character must be one of:
 *              1) Upper case alpha
 *              2) numeric
 *              3) underscore
 *
 ******************************************************************************/

u8
acpi_ut_valid_acpi_name (
	u32                     name)
{
	NATIVE_CHAR             *name_ptr = (NATIVE_CHAR *) &name;
	u32                     i;


	ACPI_FUNCTION_ENTRY ();


	for (i = 0; i < ACPI_NAME_SIZE; i++) {
		if (!((name_ptr[i] == '_') ||
			  (name_ptr[i] >= 'A' && name_ptr[i] <= 'Z') ||
			  (name_ptr[i] >= '0' && name_ptr[i] <= '9'))) {
			return (FALSE);
		}
	}

	return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_valid_acpi_character
 *
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
 *
 * DESCRIPTION: Check for a printable character
 *
 ******************************************************************************/

u8
acpi_ut_valid_acpi_character (
	NATIVE_CHAR             character)
{

	ACPI_FUNCTION_ENTRY ();

	return ((u8)   ((character == '_') ||
			   (character >= 'A' && character <= 'Z') ||
			   (character >= '0' && character <= '9')));
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_strtoul64
 *
 * PARAMETERS:  String          - Null terminated string
 *              Terminater      - Where a pointer to the terminating byte is returned
 *              Base            - Radix of the string
 *
 * RETURN:      Converted value
 *
 * DESCRIPTION: Convert a string into an unsigned value.
 *
 ******************************************************************************/
#define NEGATIVE    1
#define POSITIVE    0

acpi_status
acpi_ut_strtoul64 (
	NATIVE_CHAR             *string,
	u32                     base,
	acpi_integer            *ret_integer)
{
	u32                     index;
	acpi_integer            return_value = 0;
	acpi_status             status = AE_OK;
	acpi_integer            dividend;
	acpi_integer            quotient;


	*ret_integer = 0;

	switch (base) {
	case 0:
	case 8:
	case 10:
	case 16:
		break;

	default:
		/*
		 * The specified Base parameter is not in the domain of
		 * this function:
		 */
		return (AE_BAD_PARAMETER);
	}

	/*
	 * skip over any white space in the buffer:
	 */
	while (ACPI_IS_SPACE (*string) || *string == '\t') {
		++string;
	}

	/*
	 * If the input parameter Base is zero, then we need to
	 * determine if it is octal, decimal, or hexadecimal:
	 */
	if (base == 0) {
		if (*string == '0') {
			if (ACPI_TOLOWER (*(++string)) == 'x') {
				base = 16;
				++string;
			}
			else {
				base = 8;
			}
		}
		else {
			base = 10;
		}
	}

	/*
	 * For octal and hexadecimal bases, skip over the leading
	 * 0 or 0x, if they are present.
	 */
	if (base == 8 && *string == '0') {
		string++;
	}

	if (base == 16 &&
		*string == '0' &&
		ACPI_TOLOWER (*(++string)) == 'x') {
		string++;
	}

	/* Main loop: convert the string to an unsigned long */

	while (*string) {
		if (ACPI_IS_DIGIT (*string)) {
			index = ((u8) *string) - '0';
		}
		else {
			index = (u8) ACPI_TOUPPER (*string);
			if (ACPI_IS_UPPER ((char) index)) {
				index = index - 'A' + 10;
			}
			else {
				goto error_exit;
			}
		}

		if (index >= base) {
			goto error_exit;
		}

		/* Check to see if value is out of range: */

		dividend = ACPI_INTEGER_MAX - (acpi_integer) index;
		(void) acpi_ut_short_divide (&dividend, base, &quotient, NULL);
		if (return_value > quotient) {
			goto error_exit;
		}

		return_value *= base;
		return_value += index;
		++string;
	}

	*ret_integer = return_value;
	return (status);


error_exit:
	switch (base) {
	case 8:
		status = AE_BAD_OCTAL_CONSTANT;
		break;

	case 10:
		status = AE_BAD_DECIMAL_CONSTANT;
		break;

	case 16:
		status = AE_BAD_HEX_CONSTANT;
		break;

	default:
		/* Base validated above */
		break;
	}

	return (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_strupr
 *
 * PARAMETERS:  Src_string      - The source string to convert to
 *
 * RETURN:      Src_string
 *
 * DESCRIPTION: Convert string to uppercase
 *
 ******************************************************************************/

NATIVE_CHAR *
acpi_ut_strupr (
	NATIVE_CHAR             *src_string)
{
	NATIVE_CHAR             *string;


	ACPI_FUNCTION_ENTRY ();


	/* Walk entire string, uppercasing the letters */

	for (string = src_string; *string; ) {
		*string = (char) ACPI_TOUPPER (*string);
		string++;
	}


	return (src_string);
}

/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_mutex_initialize
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the system mutex objects.
 *
 ******************************************************************************/

acpi_status
acpi_ut_mutex_initialize (
	void)
{
	u32                     i;
	acpi_status             status;


	ACPI_FUNCTION_TRACE ("Ut_mutex_initialize");


	/*
	 * Create each of the predefined mutex objects
	 */
	for (i = 0; i < NUM_MTX; i++) {
		status = acpi_ut_create_mutex (i);
		if (ACPI_FAILURE (status)) {
			return_ACPI_STATUS (status);
		}
	}

	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_mutex_terminate
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete all of the system mutex objects.
 *
 ******************************************************************************/

void
acpi_ut_mutex_terminate (
	void)
{
	u32                     i;


	ACPI_FUNCTION_TRACE ("Ut_mutex_terminate");


	/*
	 * Delete each predefined mutex object
	 */
	for (i = 0; i < NUM_MTX; i++) {
		(void) acpi_ut_delete_mutex (i);
	}

	return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_mutex
 *
 * PARAMETERS:  Mutex_iD        - ID of the mutex to be created
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a mutex object.
 *
 ******************************************************************************/

acpi_status
acpi_ut_create_mutex (
	ACPI_MUTEX_HANDLE       mutex_id)
{
	acpi_status             status = AE_OK;


	ACPI_FUNCTION_TRACE_U32 ("Ut_create_mutex", mutex_id);


	if (mutex_id > MAX_MTX) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}


	if (!acpi_gbl_acpi_mutex_info[mutex_id].mutex) {
		status = acpi_os_create_semaphore (1, 1,
				  &acpi_gbl_acpi_mutex_info[mutex_id].mutex);
		acpi_gbl_acpi_mutex_info[mutex_id].owner_id = ACPI_MUTEX_NOT_ACQUIRED;
		acpi_gbl_acpi_mutex_info[mutex_id].use_count = 0;
	}

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_delete_mutex
 *
 * PARAMETERS:  Mutex_iD        - ID of the mutex to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a mutex object.
 *
 ******************************************************************************/

acpi_status
acpi_ut_delete_mutex (
	ACPI_MUTEX_HANDLE       mutex_id)
{
	acpi_status             status;


	ACPI_FUNCTION_TRACE_U32 ("Ut_delete_mutex", mutex_id);


	if (mutex_id > MAX_MTX) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}


	status = acpi_os_delete_semaphore (acpi_gbl_acpi_mutex_info[mutex_id].mutex);

	acpi_gbl_acpi_mutex_info[mutex_id].mutex = NULL;
	acpi_gbl_acpi_mutex_info[mutex_id].owner_id = ACPI_MUTEX_NOT_ACQUIRED;

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_acquire_mutex
 *
 * PARAMETERS:  Mutex_iD        - ID of the mutex to be acquired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire a mutex object.
 *
 ******************************************************************************/

acpi_status
acpi_ut_acquire_mutex (
	ACPI_MUTEX_HANDLE       mutex_id)
{
	acpi_status             status;
	u32                     i;
	u32                     this_thread_id;


	ACPI_FUNCTION_NAME ("Ut_acquire_mutex");


	if (mutex_id > MAX_MTX) {
		return (AE_BAD_PARAMETER);
	}


	this_thread_id = acpi_os_get_thread_id ();

	/*
	 * Deadlock prevention.  Check if this thread owns any mutexes of value
	 * greater than or equal to this one.  If so, the thread has violated
	 * the mutex ordering rule.  This indicates a coding error somewhere in
	 * the ACPI subsystem code.
	 */
	for (i = mutex_id; i < MAX_MTX; i++) {
		if (acpi_gbl_acpi_mutex_info[i].owner_id == this_thread_id) {
			if (i == mutex_id) {
				ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
						"Mutex [%s] already acquired by this thread [%X]\n",
						acpi_ut_get_mutex_name (mutex_id), this_thread_id));

				return (AE_ALREADY_ACQUIRED);
			}

			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
					"Invalid acquire order: Thread %X owns [%s], wants [%s]\n",
					this_thread_id, acpi_ut_get_mutex_name (i),
					acpi_ut_get_mutex_name (mutex_id)));

			return (AE_ACQUIRE_DEADLOCK);
		}
	}


	ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX,
			 "Thread %X attempting to acquire Mutex [%s]\n",
			 this_thread_id, acpi_ut_get_mutex_name (mutex_id)));

	status = acpi_os_wait_semaphore (acpi_gbl_acpi_mutex_info[mutex_id].mutex,
			   1, WAIT_FOREVER);
	if (ACPI_SUCCESS (status)) {
		ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX, "Thread %X acquired Mutex [%s]\n",
				 this_thread_id, acpi_ut_get_mutex_name (mutex_id)));

		acpi_gbl_acpi_mutex_info[mutex_id].use_count++;
		acpi_gbl_acpi_mutex_info[mutex_id].owner_id = this_thread_id;
	}

	else {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Thread %X could not acquire Mutex [%s] %s\n",
				 this_thread_id, acpi_ut_get_mutex_name (mutex_id),
				 acpi_format_exception (status)));
	}

	return (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_release_mutex
 *
 * PARAMETERS:  Mutex_iD        - ID of the mutex to be released
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release a mutex object.
 *
 ******************************************************************************/

acpi_status
acpi_ut_release_mutex (
	ACPI_MUTEX_HANDLE       mutex_id)
{
	acpi_status             status;
	u32                     i;
	u32                     this_thread_id;


	ACPI_FUNCTION_NAME ("Ut_release_mutex");


	this_thread_id = acpi_os_get_thread_id ();
	ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX,
		"Thread %X releasing Mutex [%s]\n", this_thread_id,
		acpi_ut_get_mutex_name (mutex_id)));

	if (mutex_id > MAX_MTX) {
		return (AE_BAD_PARAMETER);
	}


	/*
	 * Mutex must be acquired in order to release it!
	 */
	if (acpi_gbl_acpi_mutex_info[mutex_id].owner_id == ACPI_MUTEX_NOT_ACQUIRED) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
				"Mutex [%s] is not acquired, cannot release\n",
				acpi_ut_get_mutex_name (mutex_id)));

		return (AE_NOT_ACQUIRED);
	}


	/*
	 * Deadlock prevention.  Check if this thread owns any mutexes of value
	 * greater than this one.  If so, the thread has violated the mutex
	 * ordering rule.  This indicates a coding error somewhere in
	 * the ACPI subsystem code.
	 */
	for (i = mutex_id; i < MAX_MTX; i++) {
		if (acpi_gbl_acpi_mutex_info[i].owner_id == this_thread_id) {
			if (i == mutex_id) {
				continue;
			}

			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
					"Invalid release order: owns [%s], releasing [%s]\n",
					acpi_ut_get_mutex_name (i), acpi_ut_get_mutex_name (mutex_id)));

			return (AE_RELEASE_DEADLOCK);
		}
	}


	/* Mark unlocked FIRST */

	acpi_gbl_acpi_mutex_info[mutex_id].owner_id = ACPI_MUTEX_NOT_ACQUIRED;

	status = acpi_os_signal_semaphore (acpi_gbl_acpi_mutex_info[mutex_id].mutex, 1);

	if (ACPI_FAILURE (status)) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Thread %X could not release Mutex [%s] %s\n",
				 this_thread_id, acpi_ut_get_mutex_name (mutex_id),
				 acpi_format_exception (status)));
	}
	else {
		ACPI_DEBUG_PRINT ((ACPI_DB_MUTEX, "Thread %X released Mutex [%s]\n",
				 this_thread_id, acpi_ut_get_mutex_name (mutex_id)));
	}

	return (status);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_update_state_and_push
 *
 * PARAMETERS:  *Object         - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              State_list      - List the state will be added to
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a new state and push it
 *
 ******************************************************************************/

acpi_status
acpi_ut_create_update_state_and_push (
	acpi_operand_object     *object,
	u16                     action,
	acpi_generic_state      **state_list)
{
	acpi_generic_state       *state;


	ACPI_FUNCTION_ENTRY ();


	/* Ignore null objects; these are expected */

	if (!object) {
		return (AE_OK);
	}

	state = acpi_ut_create_update_state (object, action);
	if (!state) {
		return (AE_NO_MEMORY);
	}


	acpi_ut_push_generic_state (state_list, state);
	return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_pkg_state_and_push
 *
 * PARAMETERS:  *Object         - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              State_list      - List the state will be added to
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a new state and push it
 *
 ******************************************************************************/

acpi_status
acpi_ut_create_pkg_state_and_push (
	void                    *internal_object,
	void                    *external_object,
	u16                     index,
	acpi_generic_state      **state_list)
{
	acpi_generic_state       *state;


	ACPI_FUNCTION_ENTRY ();


	state = acpi_ut_create_pkg_state (internal_object, external_object, index);
	if (!state) {
		return (AE_NO_MEMORY);
	}


	acpi_ut_push_generic_state (state_list, state);
	return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_push_generic_state
 *
 * PARAMETERS:  List_head           - Head of the state stack
 *              State               - State object to push
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push a state object onto a state stack
 *
 ******************************************************************************/

void
acpi_ut_push_generic_state (
	acpi_generic_state      **list_head,
	acpi_generic_state      *state)
{
	ACPI_FUNCTION_TRACE ("Ut_push_generic_state");


	/* Push the state object onto the front of the list (stack) */

	state->common.next = *list_head;
	*list_head = state;

	return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_pop_generic_state
 *
 * PARAMETERS:  List_head           - Head of the state stack
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop a state object from a state stack
 *
 ******************************************************************************/

acpi_generic_state *
acpi_ut_pop_generic_state (
	acpi_generic_state      **list_head)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_TRACE ("Ut_pop_generic_state");


	/* Remove the state object at the head of the list (stack) */

	state = *list_head;
	if (state) {
		/* Update the list head */

		*list_head = state->common.next;
	}

	return_PTR (state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_generic_state
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a generic state object.  Attempt to obtain one from
 *              the global state cache;  If none available, create a new one.
 *
 ******************************************************************************/

acpi_generic_state *
acpi_ut_create_generic_state (void)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_ENTRY ();


	state = acpi_ut_acquire_from_cache (ACPI_MEM_LIST_STATE);

	/* Initialize */

	if (state) {
		state->common.data_type = ACPI_DESC_TYPE_STATE;
	}

	return (state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_thread_state
 *
 * PARAMETERS:  None
 *
 * RETURN:      Thread State
 *
 * DESCRIPTION: Create a "Thread State" - a flavor of the generic state used
 *              to track per-thread info during method execution
 *
 ******************************************************************************/

ACPI_THREAD_STATE *
acpi_ut_create_thread_state (
	void)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_TRACE ("Ut_create_thread_state");


	/* Create the generic state object */

	state = acpi_ut_create_generic_state ();
	if (!state) {
		return_PTR (NULL);
	}

	/* Init fields specific to the update struct */

	state->common.data_type = ACPI_DESC_TYPE_STATE_THREAD;
	state->thread.thread_id = acpi_os_get_thread_id ();

	return_PTR ((ACPI_THREAD_STATE *) state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_update_state
 *
 * PARAMETERS:  Object              - Initial Object to be installed in the
 *                                    state
 *              Action              - Update action to be performed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an "Update State" - a flavor of the generic state used
 *              to update reference counts and delete complex objects such
 *              as packages.
 *
 ******************************************************************************/

acpi_generic_state *
acpi_ut_create_update_state (
	acpi_operand_object     *object,
	u16                     action)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_TRACE_PTR ("Ut_create_update_state", object);


	/* Create the generic state object */

	state = acpi_ut_create_generic_state ();
	if (!state) {
		return_PTR (NULL);
	}

	/* Init fields specific to the update struct */

	state->common.data_type = ACPI_DESC_TYPE_STATE_UPDATE;
	state->update.object = object;
	state->update.value  = action;

	return_PTR (state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_pkg_state
 *
 * PARAMETERS:  Object              - Initial Object to be installed in the
 *                                    state
 *              Action              - Update action to be performed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a "Package State"
 *
 ******************************************************************************/

acpi_generic_state *
acpi_ut_create_pkg_state (
	void                    *internal_object,
	void                    *external_object,
	u16                     index)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_TRACE_PTR ("Ut_create_pkg_state", internal_object);


	/* Create the generic state object */

	state = acpi_ut_create_generic_state ();
	if (!state) {
		return_PTR (NULL);
	}

	/* Init fields specific to the update struct */

	state->common.data_type = ACPI_DESC_TYPE_STATE_PACKAGE;
	state->pkg.source_object = (acpi_operand_object *) internal_object;
	state->pkg.dest_object  = external_object;
	state->pkg.index        = index;
	state->pkg.num_packages = 1;

	return_PTR (state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_create_control_state
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a "Control State" - a flavor of the generic state used
 *              to support nested IF/WHILE constructs in the AML.
 *
 ******************************************************************************/

acpi_generic_state *
acpi_ut_create_control_state (
	void)
{
	acpi_generic_state      *state;


	ACPI_FUNCTION_TRACE ("Ut_create_control_state");


	/* Create the generic state object */

	state = acpi_ut_create_generic_state ();
	if (!state) {
		return_PTR (NULL);
	}


	/* Init fields specific to the control struct */

	state->common.data_type = ACPI_DESC_TYPE_STATE_CONTROL;
	state->common.state     = ACPI_CONTROL_CONDITIONAL_EXECUTING;

	return_PTR (state);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_delete_generic_state
 *
 * PARAMETERS:  State               - The state object to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Put a state object back into the global state cache.  The object
 *              is not actually freed at this time.
 *
 ******************************************************************************/

void
acpi_ut_delete_generic_state (
	acpi_generic_state      *state)
{
	ACPI_FUNCTION_TRACE ("Ut_delete_generic_state");


	acpi_ut_release_to_cache (ACPI_MEM_LIST_STATE, state);
	return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_delete_generic_state_cache
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge the global state object cache.  Used during subsystem
 *              termination.
 *
 ******************************************************************************/

void
acpi_ut_delete_generic_state_cache (
	void)
{
	ACPI_FUNCTION_TRACE ("Ut_delete_generic_state_cache");


	acpi_ut_delete_generic_cache (ACPI_MEM_LIST_STATE);
	return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_walk_package_tree
 *
 * PARAMETERS:  Obj_desc        - The Package object on which to resolve refs
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk through a package
 *
 ******************************************************************************/

acpi_status
acpi_ut_walk_package_tree (
	acpi_operand_object     *source_object,
	void                    *target_object,
	ACPI_PKG_CALLBACK       walk_callback,
	void                    *context)
{
	acpi_status             status = AE_OK;
	acpi_generic_state      *state_list = NULL;
	acpi_generic_state      *state;
	u32                     this_index;
	acpi_operand_object     *this_source_obj;


	ACPI_FUNCTION_TRACE ("Ut_walk_package_tree");


	state = acpi_ut_create_pkg_state (source_object, target_object, 0);
	if (!state) {
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	while (state) {
		this_index    = state->pkg.index;
		this_source_obj = (acpi_operand_object *)
				  state->pkg.source_object->package.elements[this_index];

		/*
		 * Check for:
		 * 1) An uninitialized package element.  It is completely
		 *    legal to declare a package and leave it uninitialized
		 * 2) Not an internal object - can be a namespace node instead
		 * 3) Any type other than a package.  Packages are handled in else
		 *    case below.
		 */
		if ((!this_source_obj) ||
			(ACPI_GET_DESCRIPTOR_TYPE (this_source_obj) != ACPI_DESC_TYPE_OPERAND) ||
			(ACPI_GET_OBJECT_TYPE (this_source_obj) != ACPI_TYPE_PACKAGE)) {
			status = walk_callback (ACPI_COPY_TYPE_SIMPLE, this_source_obj,
					 state, context);
			if (ACPI_FAILURE (status)) {
				return_ACPI_STATUS (status);
			}

			state->pkg.index++;
			while (state->pkg.index >= state->pkg.source_object->package.count) {
				/*
				 * We've handled all of the objects at this level,  This means
				 * that we have just completed a package.  That package may
				 * have contained one or more packages itself.
				 *
				 * Delete this state and pop the previous state (package).
				 */
				acpi_ut_delete_generic_state (state);
				state = acpi_ut_pop_generic_state (&state_list);

				/* Finished when there are no more states */

				if (!state) {
					/*
					 * We have handled all of the objects in the top level
					 * package just add the length of the package objects
					 * and exit
					 */
					return_ACPI_STATUS (AE_OK);
				}

				/*
				 * Go back up a level and move the index past the just
				 * completed package object.
				 */
				state->pkg.index++;
			}
		}
		else {
			/* This is a subobject of type package */

			status = walk_callback (ACPI_COPY_TYPE_PACKAGE, this_source_obj,
					  state, context);
			if (ACPI_FAILURE (status)) {
				return_ACPI_STATUS (status);
			}

			/*
			 * Push the current state and create a new one
			 * The callback above returned a new target package object.
			 */
			acpi_ut_push_generic_state (&state_list, state);
			state = acpi_ut_create_pkg_state (this_source_obj,
					   state->pkg.this_target_obj, 0);
			if (!state) {
				return_ACPI_STATUS (AE_NO_MEMORY);
			}
		}
	}

	/* We should never get here */

	return_ACPI_STATUS (AE_AML_INTERNAL);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_generate_checksum
 *
 * PARAMETERS:  Buffer          - Buffer to be scanned
 *              Length          - number of bytes to examine
 *
 * RETURN:      checksum
 *
 * DESCRIPTION: Generate a checksum on a raw buffer
 *
 ******************************************************************************/

u8
acpi_ut_generate_checksum (
	u8                      *buffer,
	u32                     length)
{
	u32                     i;
	signed char             sum = 0;

	for (i = 0; i < length; i++) {
		sum = (signed char) (sum + buffer[i]);
	}

	return ((u8) (0 - sum));
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_get_resource_end_tag
 *
 * PARAMETERS:  Obj_desc        - The resource template buffer object
 *
 * RETURN:      Pointer to the end tag
 *
 * DESCRIPTION: Find the END_TAG resource descriptor in a resource template
 *
 ******************************************************************************/


u8 *
acpi_ut_get_resource_end_tag (
	acpi_operand_object     *obj_desc)
{
	u8                      buffer_byte;
	u8                      *buffer;
	u8                      *end_buffer;


	buffer    = obj_desc->buffer.pointer;
	end_buffer = buffer + obj_desc->buffer.length;

	while (buffer < end_buffer) {
		buffer_byte = *buffer;
		if (buffer_byte & ACPI_RDESC_TYPE_MASK) {
			/* Large Descriptor - Length is next 2 bytes */

			buffer += ((*(buffer+1) | (*(buffer+2) << 8)) + 3);
		}
		else {
			/* Small Descriptor.  End Tag will be found here */

			if ((buffer_byte & ACPI_RDESC_SMALL_MASK) == ACPI_RDESC_TYPE_END_TAG) {
				/* Found the end tag descriptor, all done. */

				return (buffer);
			}

			/* Length is in the header */

			buffer += ((buffer_byte & 0x07) + 1);
		}
	}

	/* End tag not found */

	return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_report_error
 *
 * PARAMETERS:  Module_name         - Caller's module name (for error output)
 *              Line_number         - Caller's line number (for error output)
 *              Component_id        - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print error message
 *
 ******************************************************************************/

void
acpi_ut_report_error (
	NATIVE_CHAR             *module_name,
	u32                     line_number,
	u32                     component_id)
{


	acpi_os_printf ("%8s-%04d: *** Error: ", module_name, line_number);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_report_warning
 *
 * PARAMETERS:  Module_name         - Caller's module name (for error output)
 *              Line_number         - Caller's line number (for error output)
 *              Component_id        - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print warning message
 *
 ******************************************************************************/

void
acpi_ut_report_warning (
	NATIVE_CHAR             *module_name,
	u32                     line_number,
	u32                     component_id)
{

	acpi_os_printf ("%8s-%04d: *** Warning: ", module_name, line_number);
}


/*******************************************************************************
 *
 * FUNCTION:    Acpi_ut_report_info
 *
 * PARAMETERS:  Module_name         - Caller's module name (for error output)
 *              Line_number         - Caller's line number (for error output)
 *              Component_id        - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print information message
 *
 ******************************************************************************/

void
acpi_ut_report_info (
	NATIVE_CHAR             *module_name,
	u32                     line_number,
	u32                     component_id)
{

	acpi_os_printf ("%8s-%04d: *** Info: ", module_name, line_number);
}


